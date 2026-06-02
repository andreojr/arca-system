#include "main.h"
#include "app.h"
#include "config.h"
#include "com_module.h"
#include "dispatcher.h"
#include "pn532.h"
#include "pn532_stm32f4.h"

static PN532 s_nfc;

#ifdef MODULE_CONTROLLER
#include "hub.h"

extern UART_HandleTypeDef huart1;
extern uint8_t uart_rx_byte;
#endif

#ifdef MODULE_TRANSMITTER
#include "door.h"
#include "dht11.h"

extern volatile uint8_t nfc_card_ready;
#endif

void APP_Init(void)
{
    COM_Module_Init(MY_ADDR);
    DISPATCHER_Init();

    #ifdef MODULE_CONTROLLER
    NFC_HwInit(&s_nfc);
    HUB_Init(&s_nfc);
    HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1);
    HUB_RequestStatus(ADDR_ROOM_1_EXT);
    #endif

    #ifdef MODULE_TRANSMITTER
    DOOR_Init();
    NFC_SetCardCallback(DOOR_OnCardRead);
    NFC_Begin(&s_nfc);
    DHT11_init();
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
    DOOR_Process();
    #endif
}

#ifdef MODULE_CONTROLLER
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    HUB_OnUartByte(uart_rx_byte);
    HAL_UART_Receive_IT(huart, &uart_rx_byte, 1);
}
#endif