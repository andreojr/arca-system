#ifndef DOOR_H
#define DOOR_H

#include <stdint.h>

typedef enum {
    DOOR_IDLE,
    DOOR_VALIDATING,
    DOOR_UNLOCKED,
    DOOR_OPEN,
    DOOR_DENIED
} DoorState_t;

#define TIMEOUT_OPENED_MS       5000u
#define TIMEOUT_DENIED_MS       1000u
#define TIMEOUT_UNLOCKED_MS     5000u
#define TIMEOUT_VALIDATING_MS   2000u

void DOOR_Init(void);
void DOOR_OnCardRead(const uint8_t *uid, uint8_t uid_len);
void DOOR_OnStatusRequest(void);
void DOOR_OnEvent(uint8_t event, const uint8_t *payload, uint8_t payload_len);
void DOOR_Process(void);

#endif /* DOOR_H */