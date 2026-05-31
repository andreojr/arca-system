#include "nfc_module.h"
#include "pn532.h"
#include "pn532_stm32f4.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;

static PN532 pn532;
volatile uint8_t nfc_card_ready = 0;


void NFC_Module_Init(void)
{
    NFC_Begin(&pn532);
}

void NFC_Module_Process(void)
{
    if (nfc_card_ready) {
        NFC_HandleCardEvent(&pn532);
    }
}

void NFC_Module_IRQ(void)
{
    nfc_card_ready = 1;
}