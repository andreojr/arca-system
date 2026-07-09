#include "status_poll.h"
#include "config.h"
#include "door.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Espalha o instante do primeiro envio entre 0..INTERVAL-1 conforme o
 * endereço deste módulo, pra reduzir a chance de duas DOORs transmitirem
 * status ao mesmo tempo (ex.: após um reset simultâneo). Os envios
 * seguintes de cada módulo continuam espaçados por INTERVAL a partir daí. */
static uint32_t _jitter_offset_ms(void)
{
    return ((uint32_t)MY_ADDR * 977u) % STATUS_UPDATE_INTERVAL_MS;
}

static uint32_t s_last_tx     = 0;
static uint8_t  s_initialized = 0;

void StatusPoll_Process(void)
{
    uint32_t now = HAL_GetTick();

    if (!s_initialized) {
        s_last_tx     = now - STATUS_UPDATE_INTERVAL_MS + _jitter_offset_ms();
        s_initialized = 1;
    }

    if ((now - s_last_tx) < STATUS_UPDATE_INTERVAL_MS) return;

    s_last_tx = now;
    DOOR_SendStatusUpdate();
}
