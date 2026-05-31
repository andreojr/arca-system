#include "hub.h"
#include "com_module.h"
#include "config.h"
#include <stdio.h>

void HUB_OnEvent(uint8_t event, uint8_t src, const uint8_t *payload, uint8_t payload_len)
{
    switch (event) {
        case EVENT_AUTHORIZE:
            printf("[HUB] AUTHORIZE de 0x%02X\r\n", src);
            COM_Module_Send(src, EVENT_ACCESS_GRANTED, NULL, 0);
            // COM_Module_Send(src, EVENT_ACCESS_DENIED, NULL, 0);
            break;

        default:
            break;
    }
}