#ifndef DHT11_H
#define DHT11_H

#include "stm32f4xx_hal.h"

#define DHT11_SUCCESS          1
#define DHT11_ERROR_CHECKSUM   2
#define DHT11_ERROR_TIMEOUT    3

typedef struct {
    uint8_t temperature;
    uint8_t humidity;
} DHT11_Data_t;

void DHT11_init(void);
int  DHT11_read(DHT11_Data_t* dht11_data);

#endif