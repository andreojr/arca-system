#include "door.h"
#include "com_module.h"
#include "dht11.h"
#include "main.h"
#include "press.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "stm32f4xx_hal_gpio.h"
#include "pn532_stm32f4.h"
#include "cc1101.h"
#include "uart_protocol.h"

static DoorState_t s_state = DOOR_IDLE;
static uint32_t s_timer = 0;
static uint8_t s_uid[7] = {0};
static uint8_t s_uid_len = 0;
static DHT11_Data_t dht11_data;
static uint8_t s_buzzer_on = 0;
static uint8_t s_my_addr_effective = MY_ADDR;
static PN532  *s_pn532 = NULL;

static void _on_access_confirmed(void)
{
    if (s_state != DOOR_UNLOCKED) return;

    s_timer = HAL_GetTick();

    COM_TxPacket_t pkt = {
        .dst = ADDR_CONTROLLER,
        .event = EVENT_ACCESS_CONFIRMED,
        .payload_len = s_uid_len
    };
    memcpy(pkt.payload, s_uid, s_uid_len);
    COM_Module_Send(&pkt);

    printf("[DOOR] ACCESS CONFIRMED como 0x%02X\r\n", s_my_addr_effective);
}

static void _toggle_direction_address(void)
{
    uint8_t room = (s_my_addr_effective >> 4) & 0x0F;
    uint8_t dir  = s_my_addr_effective & 0x0F;
    dir = (dir == 1) ? 2 : 1;
    s_my_addr_effective = (room << 4) | dir;

    COM_Module_SetAddr(s_my_addr_effective);
}

static void _run_test_battery(void)
{
    HAL_GPIO_WritePin(LED_DENIED_GPIO_Port, LED_DENIED_Pin, GPIO_PIN_SET);
    HAL_Delay(TEST_BATTERY_STEP_MS);
    HAL_GPIO_WritePin(LED_DENIED_GPIO_Port, LED_DENIED_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LED_GRANTED_GPIO_Port, LED_GRANTED_Pin, GPIO_PIN_SET);
    HAL_Delay(TEST_BATTERY_STEP_MS);
    HAL_GPIO_WritePin(LED_GRANTED_GPIO_Port, LED_GRANTED_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
    HAL_Delay(TEST_BATTERY_STEP_MS);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

static void _send_status(void)
{
    uint8_t fw[4]      = {0};
    uint8_t pn532_ok   = (PN532_GetFirmwareVersion(s_pn532, fw) == PN532_STATUS_OK) ? 1 : 0;
    uint8_t cc1101_ver = TI_read_status(CCxxx0_VERSION);

    DHT11_Data_t dht = {0};
    uint8_t dht_ok  = (DHT11_read(&dht) == DHT11_SUCCESS) ? 1 : 0;
    uint8_t pressed = Press_IsDoorPressed() ? 1 : 0;

    uint8_t resp[10] = {
        pn532_ok,
        fw[1],                  /* PN532 fw version   */
        fw[2],                  /* PN532 fw revision  */
        cc1101_ver,             /* CC1101 VERSION reg */
        s_my_addr_effective,    /* endereco deste no  */
        dht_ok,
        dht.temperature,
        dht.humidity,
        1,                      /* leitura do sensor de forca executada */
        pressed,
    };
    Protocol_Send(resp, sizeof(resp));

    _run_test_battery();
}

static void _send_identity(void)
{
    uint8_t resp[1] = { s_my_addr_effective };
    Protocol_Send(resp, sizeof(resp));
}

void DOOR_Init(PN532 *pn532)
{
    s_pn532 = pn532;
    s_state = DOOR_IDLE;
    Press_Init();
    printf("[DOOR] IDLE\r\n");
}

void DOOR_OnCardRead(const uint8_t *uid, uint8_t uid_len)
{
    if (s_state != DOOR_IDLE) return;

    s_uid_len = uid_len;
    memcpy(s_uid, uid, uid_len);

    s_state = DOOR_VALIDATING;
    s_timer = HAL_GetTick();
    printf("[DOOR] VALIDATING\r\n");

    COM_TxPacket_t pkt = {
        .dst = ADDR_CONTROLLER,
        .event = EVENT_AUTHORIZE,
        .payload_len = s_uid_len
    };
    memcpy(pkt.payload, s_uid, s_uid_len);

    COM_Module_Send(&pkt);
}

void DOOR_SendStatusUpdate(void)
{
    DHT11_read(&dht11_data);

    uint8_t payload[4];
    payload[0] = dht11_data.temperature;
    payload[1] = dht11_data.humidity;
    payload[2] = s_state == DOOR_OPEN ? 1u : 0u;

    COM_TxPacket_t pkt = {
        .dst = ADDR_CONTROLLER,
        .event = EVENT_STATUS_UPDATE,
        .payload_len = 3
    };
    memcpy(pkt.payload, payload, 3);
    COM_Module_Send(&pkt);
    printf("[DOOR] STATUS_UPDATE enviado\r\n");
}

void DOOR_OnAuthorizeResponse(uint8_t event, const uint8_t *payload, uint8_t payload_len)
{
    if (s_state != DOOR_VALIDATING) return;

    switch (event) {
        case EVENT_ACCESS_GRANTED:
            HAL_GPIO_WritePin(LED_GRANTED_GPIO_Port, LED_GRANTED_Pin, GPIO_PIN_SET);
            s_state = DOOR_UNLOCKED;
            printf("[DOOR] UNLOCKED\r\n");
            break;
        case EVENT_ACCESS_DENIED:
            HAL_GPIO_WritePin(LED_DENIED_GPIO_Port, LED_DENIED_Pin, GPIO_PIN_SET);
            s_state = DOOR_DENIED;
            printf("[DOOR] DENIED\r\n");
            break;
        default: break;
    }
}

void DOOR_Process(void)
{
    uint32_t now = HAL_GetTick();

    Protocol_Frame_t frame;
    if (Protocol_Receive(&frame) && frame.length > 0) {
        uint8_t cmd = frame.data[0];
        if (cmd == 'V' || cmd == 'v') {
            _send_status();
        } else if (cmd == 'I' || cmd == 'i') {
            _send_identity();
        }
    }

    switch (s_state) {
        case DOOR_IDLE:
            if (Press_IsDoorPressed()) {
                s_state = DOOR_DENIED;
                s_timer = HAL_GetTick();
                HAL_GPIO_WritePin(LED_DENIED_GPIO_Port, LED_DENIED_Pin, GPIO_PIN_SET);
                printf("[DOOR] FORCED OPEN -> DENIED\r\n");
            }
            break;
        case DOOR_VALIDATING:
            if ((now - s_timer) >= TIMEOUT_VALIDATING_MS) {
                s_state = DOOR_DENIED;
                printf("[DOOR] TIMEOUT -> DENIED\r\n");
            }
            break;

        case DOOR_UNLOCKED:
            if ((now - s_timer) >= TIMEOUT_UNLOCKED_MS) {
                HAL_GPIO_WritePin(LED_GRANTED_GPIO_Port, LED_GRANTED_Pin, GPIO_PIN_RESET);
                s_state = DOOR_IDLE;
                printf("[DOOR] IDLE\r\n");
            } else if (Press_IsDoorPressed()) {
                _on_access_confirmed();
                s_state = DOOR_OPEN;
                s_timer = HAL_GetTick();
                printf("[DOOR] OPEN\r\n");
            }
            break;
        
        case DOOR_OPEN:
            if (!Press_IsDoorPressed()) {
                s_buzzer_on = 0;
                HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(LED_GRANTED_GPIO_Port, LED_GRANTED_Pin, GPIO_PIN_RESET);
                _toggle_direction_address();
                s_state = DOOR_IDLE;
                printf("[DOOR] DOOR CLOSED -> IDLE\r\n");
                break;
            }
            
            if (!s_buzzer_on && (now - s_timer) >= TIMEOUT_OPENED_MS) {
                s_buzzer_on = 1;
                HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
                printf("[DOOR] OPEN TOO LONG -> ALARM\r\n");
            }
            break;

        case DOOR_DENIED:
            if ((now - s_timer) >= TIMEOUT_DENIED_MS) {
                HAL_GPIO_WritePin(LED_DENIED_GPIO_Port, LED_DENIED_Pin, GPIO_PIN_RESET);
                s_state = DOOR_IDLE;
                printf("[DOOR] IDLE\r\n");
            }
            break;

        default:
            break;
    }
}