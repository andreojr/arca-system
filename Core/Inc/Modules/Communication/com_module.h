#ifndef COM_MODULE_H
#define COM_MODULE_H

#include <stdint.h>

typedef struct {
    uint8_t dst;
    uint8_t event;
    uint8_t payload[59];
    uint8_t payload_len;
} COM_TxPacket_t;

#define COM_TIMEOUT 200u

typedef void (*COM_RxCallback_t)(uint8_t event, uint8_t src, const uint8_t *payload, uint8_t payload_len);

void COM_Module_Init(uint8_t my_addr);
void COM_Module_SetAddr(uint8_t new_addr);
void COM_Module_Send(const COM_TxPacket_t *pkt);
void COM_Module_Process(void);
void COM_Module_SetRxCallback(COM_RxCallback_t cb);

#endif /* COM_MODULE_H */