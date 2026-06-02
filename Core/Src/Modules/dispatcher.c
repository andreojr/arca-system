// dispatcher.c
#include "dispatcher.h"
#include "com_module.h"
#include "config.h"

#if defined(MODULE_CONTROLLER)
#include "hub.h"

static void _on_rx(uint8_t event, uint8_t src, const uint8_t *payload, uint8_t payload_len)
{
    switch (event) {
        case EVENT_AUTHORIZE:
            HUB_OnAuthorizeRequest(src, payload, payload_len);
            break;
        case EVENT_STATUS_RESPONSE:
            HUB_OnStatusResponse(src, payload, payload_len);
            break;
        case EVENT_ACCESS_CONFIRMED:
            HUB_OnAccessConfirmed(src, payload, payload_len);
            break;
        default: break;
    }
}

#elif defined(MODULE_TRANSMITTER)
#include "door.h"

static void _on_rx(uint8_t event, uint8_t src, const uint8_t *payload, uint8_t payload_len)
{
    switch (event) {
        case EVENT_ACCESS_GRANTED:
        case EVENT_ACCESS_DENIED:
            DOOR_OnAuthorizeResponse(event, payload, payload_len);
            break;
        case EVENT_STATUS_REQUEST:
            DOOR_OnStatusRequest();
            break;
        default: break;
    }
}
#endif

void DISPATCHER_Init(void)
{
    COM_Module_SetRxCallback(_on_rx);
}