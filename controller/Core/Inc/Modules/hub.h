#ifndef HUB_H
#define HUB_H

#include <stdint.h>
#include "pn532.h"

typedef enum {
    HUB_IDLE,
    HUB_ENROLLING,
    HUB_DELETING,
} HubState_t;

void HUB_Init(PN532 *pn532);
void HUB_Process(void);
void HUB_OnUartByte(uint8_t byte);

void HUB_OnAuthorizeRequest(uint8_t src, const uint8_t *uid, uint8_t uid_len);
void HUB_OnStatusResponse(uint8_t src, const uint8_t *payload, uint8_t payload_len);
void HUB_RequestStatus(uint8_t dst);

#endif /* HUB_H */