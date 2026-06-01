#include "door.h"
#include "com_module.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include "config.h"

static DoorState_t s_state = DOOR_IDLE;
static uint32_t s_timer = 0;
static uint8_t s_uid[7] = {0};
static uint8_t s_uid_len = 0;

void DOOR_Init(void)
{
    s_state = DOOR_IDLE;
    printf("[DOOR] IDLE\r\n");
}

void DOOR_OnCardRead(const uint8_t *uid, uint8_t uid_len)
{
    if (s_state != DOOR_IDLE) return;

    s_uid_len = uid_len;
    for (uint8_t i = 0; i < uid_len; i++)
        s_uid[i] = uid[i];

    s_state = DOOR_VALIDATING;
    s_timer = HAL_GetTick();
    printf("[DOOR] VALIDATING\r\n");
    COM_Module_Send(ADDR_CONTROLLER, EVENT_AUTHORIZE, s_uid, s_uid_len);
}

void DOOR_OnStatusRequest(void)
{
    printf("[DOOR] STATUS_REQUEST recebido\r\n");

    /* TODO: substituir por leitura real do DHT11 */
    uint8_t payload[4];
    uint8_t temp     = 25;  /* 25°C */
    uint8_t humidity = 60;  /* 60%RH */

    payload[0] = temp;
    payload[1] = 0x00;      /* decimal — sempre 0 no DHT11 */
    payload[2] = humidity;
    payload[3] = 0x00;      /* decimal — sempre 0 no DHT11 */

    COM_Module_Send(ADDR_CONTROLLER, EVENT_STATUS_RESPONSE, payload, 4);
    printf("[DOOR] STATUS_RESPONSE enviado\r\n");
}

void DOOR_OnEvent(uint8_t event, const uint8_t *payload, uint8_t payload_len)
{
    if (s_state != DOOR_VALIDATING) return;

    switch (event) {
        case EVENT_ACCESS_GRANTED:
            s_state = DOOR_UNLOCKED;
            printf("[DOOR] UNLOCKED\r\n");
            break;
        case EVENT_ACCESS_DENIED:
            s_state = DOOR_DENIED;
            printf("[DOOR] DENIED\r\n");
            break;
        default: break;
    }
}

void DOOR_Process(void)
{
    uint32_t now = HAL_GetTick();

    switch (s_state) {
        case DOOR_VALIDATING:
            if ((now - s_timer) >= TIMEOUT_VALIDATING_MS) {
                s_state = DOOR_DENIED;
                printf("[DOOR] TIMEOUT -> DENIED\r\n");
            }
            break;

        case DOOR_UNLOCKED:
            s_state = DOOR_IDLE;
            printf("[DOOR] IDLE\r\n");
            break;

        case DOOR_DENIED:
            if ((now - s_timer) >= TIMEOUT_DENIED_MS) {
                s_state = DOOR_IDLE;
                printf("[DOOR] IDLE\r\n");
            }
            break;

        default:
            break;
    }
}