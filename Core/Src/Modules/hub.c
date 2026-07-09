#include "hub.h"
#include "com_module.h"
#include "config.h"
#include "flash_db.h"
#include "pn532_stm32f4.h"
#include "stm32f4xx_hal.h"
#include "ihm.h"
#include "uart_protocol.h"
#include "rtc_sync.h"
#include "rtc_api.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TIMEOUT_ENROLLING_MS        10000u
#define TIMEOUT_DELETING_MS         10000u

static HubState_t       s_state                  = HUB_IDLE;
static uint32_t         s_timer                  = 0;
static PN532           *s_pn532                  = NULL;
static volatile uint8_t s_start_detection_pending = 0;

extern volatile uint8_t nfc_card_ready;

/* ── Callbacks internos de NFC ─────────────────────────── */

static void _on_enroll_card(const uint8_t *uid, uint8_t uid_len)
{
    uint32_t idx = FlashDB_UserFindUID(uid, uid_len);
    if (idx != MAX_FLASH_RECORDS) {
        printf("[CLI][HUB] ENROLLING — cartao ja cadastrado\r\n");
        return;
    }

    FlashUser_t user = {0};
    memcpy(user.uid, uid, uid_len);
    user.uid_len    = uid_len;
    user.access_lvl = 1;
    strncpy(user.name, "default", sizeof(user.name) - 1);

    if (FlashDB_UserAdd(&user)) {
        printf("[CLI][HUB] ENROLLING — cartao cadastrado\r\n");
    } else {
        printf("[CLI][HUB] ENROLLING — erro ao gravar flash\r\n");
    }
}

static void _on_delete_card(const uint8_t *uid, uint8_t uid_len)
{
    uint32_t idx = FlashDB_UserFindUID(uid, uid_len);
    if (idx == MAX_FLASH_RECORDS) {
        printf("[CLI][HUB] DELETING — cartao nao encontrado\r\n");
        return;
    }

    if (FlashDB_UserDelete(idx)) {
        printf("[CLI][HUB] DELETING — cartao removido\r\n");
    } else {
        printf("[CLI][HUB] DELETING — erro ao deletar\r\n");
    }
}

/* ── API pública ───────────────────────────────────────── */

void HUB_Init(PN532 *pn532)
{
    s_pn532 = pn532;
    s_state = HUB_IDLE;
    IHM_Init();
    printf("[HUB] IDLE\r\n");
}

void HUB_OnUartByte(const Protocol_Frame_t *frame)
{
    if (s_state != HUB_IDLE) return;

    uint8_t cmd = frame->data[0];

    if (cmd == 'C' || cmd == 'c') {
        s_state = HUB_ENROLLING;
        s_timer = HAL_GetTick();
        NFC_SetCardCallback(_on_enroll_card);
        s_start_detection_pending = 1;  /* SPI proibido em ISR — deferido ao HUB_Process */
        printf("[CLI][HUB] ENROLLING — aproxime o cartao (10s)\r\n");
    }
    else if (cmd == 'D' || cmd == 'd') {
        s_state = HUB_DELETING;
        s_timer = HAL_GetTick();
        NFC_SetCardCallback(_on_delete_card);
        s_start_detection_pending = 1;  /* SPI proibido em ISR — deferido ao HUB_Process */
        printf("[CLI][HUB] DELETING — aproxime o cartao (10s)\r\n");
    }
}

void HUB_Process(void)
{
    uint32_t now = HAL_GetTick();

    IHM_Process();   /* devolve a tela ao repouso após o timeout */

    Protocol_Frame_t frame;
    if (Protocol_Receive(&frame) && frame.length > 0) {
        uint8_t cmd = frame.data[0];
        if (cmd == 'C' || cmd == 'c' || cmd == 'D' || cmd == 'd')
            HUB_OnUartByte(&frame);
        else
            RTC_API_Process(&frame);
    }

    if (s_start_detection_pending) {
        s_start_detection_pending = 0;
        NFC_IRQ_Disarm();
        NFC_StartDetection(s_pn532);
        NFC_IRQ_Arm();
    }

    switch (s_state) {
        case HUB_IDLE:
            break;

        case HUB_ENROLLING:
        case HUB_DELETING:
            if ((now - s_timer) >= TIMEOUT_ENROLLING_MS) {
                NFC_StopScan(s_pn532);
                s_state = HUB_IDLE;
                printf("[CLI][HUB] timeout -> IDLE\r\n");
                break;
            }
            if (nfc_card_ready) {
                NFC_HandleCardEventOnce(s_pn532);
                s_state = HUB_IDLE;
                printf("[HUB] IDLE\r\n");
            }
            break;
    }
}

void HUB_OnAuthorizeRequest(uint8_t src, const uint8_t *uid, uint8_t uid_len)
{
    printf("[HUB] AUTHORIZE de 0x%02X\r\n", src);
    uint32_t idx = FlashDB_UserFindUID(uid, uid_len);
    
    COM_TxPacket_t pkt = {0};
    pkt.dst = src;

    uint8_t granted = (idx != MAX_FLASH_RECORDS);
    pkt.event = granted ? EVENT_ACCESS_GRANTED : EVENT_ACCESS_DENIED;

    /* Envia a resposta pelo rádio ANTES de qualquer desenho no display:
     * IHM_ShowAccessEvent() faz várias escritas SPI no ILI9341 e podia
     * atrasar o pacote além do TIMEOUT_VALIDATING_MS do door. */
    COM_Module_Send(&pkt);

    if (granted) {
        printf("[HUB] GRANTED -> 0x%02X\r\n", src);
        /* Display só atualiza em HUB_OnAccessConfirmed, quando o usuário
         * de fato abre a porta. */
    } else {
        printf("[HUB] DENIED -> 0x%02X\r\n", src);
    }
}

void HUB_OnAccessConfirmed(uint8_t src, const uint8_t *uid, uint8_t uid_len)
{
    uint8_t room      = (src >> 4) & 0x0F;
    uint8_t direction = src & 0x0F;

    if (room == 0) return;

    uint32_t idx = FlashDB_UserFindUID(uid, uid_len);
    if (idx == MAX_FLASH_RECORDS) return;

    const char *dir_str = (direction == 1) ? "ENTRADA" : "SAIDA";

    printf("[HUB] Sala %u %s — UID:", room, dir_str);
    for (uint8_t i = 0; i < uid_len; i++)
        printf(" %02X", uid[i]);
    printf("\r\n");

    RTC_ReadCurrent();
    char timestamp[24];
    snprintf(timestamp, sizeof(timestamp), "%02u:%02u:%02u %02u/%02u/%02u",
             rtc_status.hours, rtc_status.minutes, rtc_status.seconds,
             rtc_status.day, rtc_status.month, rtc_status.year % 100);

    IHM_ShowAccessEvent(room, direction, uid, uid_len, timestamp);
}

void HUB_OnStatusUpdate(uint8_t src, const uint8_t *payload, uint8_t payload_len)
{
    if (payload_len < 3) return;
    uint8_t temp     = payload[0];
    uint8_t humidity = payload[1];
    uint8_t door     = payload[2];
    printf("[HUB] STATUS_UPDATE de 0x%02X — temp=%u grC umidade=%u%%\r\n — Door: %s\r\n",
           src, temp, humidity, door ? "ABERTA" : "FECHADA");

    IHM_Status_t st = {
        .src      = src,
        .temp     = temp,
        .humidity = humidity,
        .door     = door,
    };
    IHM_UpdateStatus(&st);
}