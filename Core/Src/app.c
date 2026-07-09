#include "main.h"
#include "app.h"
#include "config.h"
#include "com_module.h"
#include "dispatcher.h"
#include "pn532.h"
#include "pn532_stm32f4.h"
#include "ili9341.h"

static PN532 s_nfc;

#ifdef MODULE_CONTROLLER
#include "hub.h"
#include "uart_protocol.h"

extern UART_HandleTypeDef huart1;
#endif

#ifdef MODULE_TRANSMITTER
#include "door.h"
#include "dht11.h"
#include "uart_protocol.h"

extern volatile uint8_t nfc_card_ready;
extern TIM_HandleTypeDef htim10;
extern UART_HandleTypeDef huart1;

static volatile uint8_t s_status_pending = 0;

/* Espalha o instante do primeiro STATUS_UPDATE conforme o endereço deste
 * módulo, pra reduzir a chance de duas DOORs transmitirem ao mesmo tempo
 * (ex.: após um reset simultâneo). Os envios seguintes ficam a cargo do
 * próprio TIM10, que recarrega em STATUS_UPDATE_INTERVAL_MS a partir daí. */
static uint32_t _status_jitter_offset_ms(void)
{
    return ((uint32_t)MY_ADDR * 977u) % STATUS_UPDATE_INTERVAL_MS;
}
#endif

void APP_Init(void)
{
    ILI9341_Unselect(); // deselect display CS before any SPI activity (see ili9341.h)
    COM_Module_Init(MY_ADDR);
    DISPATCHER_Init();

    #ifdef MODULE_CONTROLLER
    NFC_HwInit(&s_nfc);
    HUB_Init(&s_nfc);
    Protocol_Init(&huart1);
    #endif

    #ifdef MODULE_TRANSMITTER
    DOOR_Init(&s_nfc);
    NFC_SetCardCallback(DOOR_OnCardRead);
    NFC_Begin(&s_nfc);
    DHT11_init();
    Protocol_Init(&huart1);

    __HAL_TIM_SET_AUTORELOAD(&htim10, STATUS_UPDATE_INTERVAL_MS - 1);
    __HAL_TIM_SET_COUNTER(&htim10, STATUS_UPDATE_INTERVAL_MS - 1 - _status_jitter_offset_ms());
    HAL_TIM_Base_Start_IT(&htim10);
    #endif
}

void APP_Run(void)
{
    COM_Module_Process();

    #ifdef MODULE_CONTROLLER
    HUB_Process();
    #endif

    #ifdef MODULE_TRANSMITTER
    if (nfc_card_ready) {
        NFC_HandleCardEvent(&s_nfc);
    }
    if (s_status_pending) {
        s_status_pending = 0;
        DOOR_SendStatusUpdate();
    }
    DOOR_Process();
    #endif
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    Protocol_UART_RxCallback();
}

#ifdef MODULE_TRANSMITTER

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM10) {
        s_status_pending = 1;
    }
}

#endif