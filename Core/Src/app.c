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
#include "status_poll.h"

extern volatile uint8_t nfc_card_ready;
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
    StatusPoll_Process();
    #endif
}

#ifdef MODULE_CONTROLLER
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    Protocol_UART_RxCallback();
}
#endif