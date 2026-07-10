#ifndef IHM_H
#define IHM_H

#include <stdint.h>

#define IHM_HEADER_H           40u
#define IHM_BODY_Y             (IHM_HEADER_H + 8u)
#define IHM_STATUS_Y           (IHM_BODY_Y + 60u)

#define IHM_ICON_X             8u
#define IHM_TEXT_X             48u
#define IHM_ICON_SIZE          32u

/* Linha de "Sala n (INT/EXT)" fica sem ícone; as 3 linhas de status
 * (temp/umidade/porta) precisam de espaço pro ícone de 32px. */
#define IHM_TEMP_Y             (IHM_STATUS_Y + 18u)
#define IHM_HUMIDITY_Y         (IHM_TEMP_Y + 35u)
#define IHM_DOOR_Y             (IHM_HUMIDITY_Y + 35u)

#define IHM_TEMP_LIMIAR_FRIO    24u
#define IHM_TEMP_LIMIAR_QUENTE  28u

/* Tempo que um evento permanece na tela antes de voltar ao repouso. */
#define IHM_EVENT_TIMEOUT_MS   5000u

/* Barra de progresso (linha inferior): cresce da esquerda pra direita ao
 * longo de STATUS_UPDATE_INTERVAL_MS, zerando a cada novo status recebido. */
#define IHM_BAR_MARGIN         8u
#define IHM_BAR_H              6u
#define IHM_BAR_Y              (ILI9341_HEIGHT - 12u)
#define IHM_BAR_W              (ILI9341_WIDTH - 2u * IHM_BAR_MARGIN)

/* Dados ambientais de uma sala, recebidos via EVENT_STATUS_UPDATE. */
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

/* Mostra um evento de acesso confirmado (entrada/saída de uma sala).
 * `timestamp` (pode ser NULL) é desenhado abaixo do UID, já formatado
 * (ver RTC_ReadCurrent / rtc_status em rtc_sync.h). */
void IHM_ShowAccessEvent(uint8_t room, uint8_t direction,
                         const uint8_t *uid, uint8_t uid_len,
                         const char *timestamp);

/* Deve ser chamada periodicamente (no loop, via HUB_Process). Após
 * IHM_EVENT_TIMEOUT_MS sem evento novo, a tela volta ao repouso. */
void IHM_Process(void);

#endif /* IHM_H */