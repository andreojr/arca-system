#ifndef IHM_H
#define IHM_H

#include <stdint.h>

/* Dados ambientais de uma sala, recebidos via EVENT_STATUS_RESPONSE. */
typedef struct {
    uint8_t src;       /* endereço de origem (0x?1/0x?2) */
    uint8_t temp;      /* temperatura em °C              */
    uint8_t humidity;  /* umidade relativa em %          */
    uint8_t door;      /* 0 = FECHADA, 1 = ABERTA        */
} IHM_Status_t;

/* Inicializa o display MSP2402 e desenha a tela base (estado de repouso). */
void IHM_Init(void);

/* Desenha o estado de repouso ("Aguardando..."). */
void IHM_ShowIdle(void);

/* Atualiza o bloco de status ambiental (temp/umidade/porta). */
void IHM_UpdateStatus(const IHM_Status_t *st);

/* Mostra um evento de acesso confirmado (entrada/saída de uma sala). */
void IHM_ShowAccessEvent(uint8_t room, uint8_t direction,
                         const uint8_t *uid, uint8_t uid_len);

/* Deve ser chamada periodicamente (no loop, via HUB_Process). Após
 * IHM_EVENT_TIMEOUT_MS sem evento novo, a tela volta ao repouso. */
void IHM_Process(void);

#endif /* IHM_H */