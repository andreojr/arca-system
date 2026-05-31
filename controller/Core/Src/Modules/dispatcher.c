#include "dispatcher.h"
#include "com_module.h"
#include "hub.h"
#include "config.h"

static void _on_rx(uint8_t event, uint8_t src, const uint8_t *payload, uint8_t payload_len)
{
    switch (event) {
        case EVENT_AUTHORIZE:
            HUB_OnEvent(event, src, payload, payload_len);
            break;

        default:
            break;
    }
}

void DISPATCHER_Init(void)
{
    COM_Module_SetRxCallback(_on_rx);
}