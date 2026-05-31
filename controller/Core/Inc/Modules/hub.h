#ifndef HUB_H
#define HUB_H

#include <stdint.h>

void HUB_OnEvent(uint8_t event, uint8_t src, const uint8_t *payload, uint8_t payload_len);

#endif /* HUB_H */