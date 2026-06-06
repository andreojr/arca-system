#include "ihm.h"
#include "ili9341.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── IHM — Display MSP2402 (ILI9341, 240x320) ──────────────
 *
 * Layout em modo retrato (240 col x 320 lin):
 *
 *   +----------------------------------+  y=0
 *   |            A.R.C.A.              |   cabeçalho (faixa, fixo)
 *   +----------------------------------+  y=40
 *   |  SALA n  ENTRADA / SAIDA        |   linha de evento  ─┐
 *   |  UID: XX XX XX XX               |                     │ corpo
 *   |                                  |                     │ (limpo no
 *   |  Temp:    NN C                   |   bloco de status   │  repouso)
 *   |  Umidade: NN %                   |                     │
 *   |  Porta:   ABERTA / FECHADA       |                    ─┘
 *   +----------------------------------+
 *
 * Comportamento: o cabeçalho é fixo. O corpo mostra o último evento
 * (acesso ou status) e, após IHM_EVENT_TIMEOUT_MS sem evento novo,
 * volta sozinho ao estado de repouso ("Aguardando..."). A retroiluminação
 * permanece ligada o tempo todo.
 */
#define IHM_HEADER_H           40u
#define IHM_BODY_Y             (IHM_HEADER_H + 8u)
#define IHM_STATUS_Y           (IHM_BODY_Y + 60u)

/* Tempo que um evento permanece na tela antes de voltar ao repouso. */
#define IHM_EVENT_TIMEOUT_MS   5000u

static uint8_t  s_ready         = 0;  /* display inicializado            */
static uint8_t  s_showing_event = 0;  /* há evento na tela aguardando expirar */
static uint32_t s_event_ts      = 0;  /* HAL_GetTick() do último evento  */

/* Marca que um evento acabou de ser desenhado (rearma o timer). */
static void _mark_event(void)
{
    s_event_ts      = HAL_GetTick();
    s_showing_event = 1;
}

static void _clear_event_area(void)
{
    ILI9341_FillRectangle(0, IHM_HEADER_H, ILI9341_WIDTH,
                          IHM_STATUS_Y - IHM_HEADER_H, ILI9341_BLACK);
}

static void _clear_status_area(void)
{
    ILI9341_FillRectangle(0, IHM_STATUS_Y, ILI9341_WIDTH,
                          ILI9341_HEIGHT - IHM_STATUS_Y, ILI9341_BLACK);
}

/* static void _clear_body(void)
{
    _clear_event_area();
    _clear_status_area();
} */

static void _draw_header(void)
{
    ILI9341_FillScreen(ILI9341_BLACK);
    ILI9341_FillRectangle(0, 0, ILI9341_WIDTH, IHM_HEADER_H, ILI9341_BLUE);
    ILI9341_WriteString(70, 12, "A.R.C.A.", Font_11x18, ILI9341_WHITE, ILI9341_BLUE);
}

void IHM_ShowIdle(void)
{
    if (!s_ready) return;
    _clear_event_area();
    ILI9341_WriteString(8, IHM_BODY_Y, "Aguardando...", Font_11x18,
                        ILI9341_WHITE, ILI9341_BLACK);
    s_showing_event = 0;
}

void IHM_Init(void)
{
    HAL_GPIO_WritePin(IHM_LED_GPIO_Port, IHM_LED_Pin, GPIO_PIN_SET); // turn on backlight
    ILI9341_Init();
    _draw_header();
    s_ready = 1;
    IHM_ShowIdle();
}


void IHM_ShowAccessEvent(uint8_t room, uint8_t direction,
                         const uint8_t *uid, uint8_t uid_len)
{
    if (!s_ready) return;

    char line[32];
    uint16_t color      = (direction == 1) ? ILI9341_GREEN : ILI9341_CYAN;
    const char *dir_str = (direction == 1) ? "ENTRADA" : "SAIDA";

    _clear_event_area();

    snprintf(line, sizeof(line), "SALA %u  %s", room, dir_str);
    ILI9341_WriteString(8, IHM_BODY_Y, line, Font_11x18, color, ILI9341_BLACK);

    /* UID em hexadecimal (até 7 bytes -> "XX " = 21 chars) */
    char uid_str[24] = {0};
    size_t pos = 0;
    for (uint8_t i = 0; i < uid_len && pos + 3 < sizeof(uid_str); i++)
        pos += snprintf(uid_str + pos, sizeof(uid_str) - pos, "%02X ", uid[i]);

    ILI9341_WriteString(8,  IHM_BODY_Y + 24, "UID:", Font_7x10,
                        ILI9341_WHITE, ILI9341_BLACK);
    ILI9341_WriteString(40, IHM_BODY_Y + 24, uid_str, Font_7x10,
                        ILI9341_YELLOW, ILI9341_BLACK);

    _mark_event();
}

void IHM_UpdateStatus(const IHM_Status_t *st)
{
    if (!s_ready || st == NULL) return;

    char line[32];

    _clear_status_area();

     /*snprintf(line, sizeof(line), "Sala 0x%02X", st->src);
    ILI9341_WriteString(8, IHM_STATUS_Y, line, Font_7x10,
                        ILI9341_WHITE, ILI9341_BLACK);*/
 
    snprintf(line, sizeof(line), "Temp:    %u C", st->temp);
    ILI9341_WriteString(8, IHM_STATUS_Y + 16, line, Font_11x18,
                        ILI9341_WHITE, ILI9341_BLACK);
 
    snprintf(line, sizeof(line), "Umidade: %u %%", st->humidity);
    ILI9341_WriteString(8, IHM_STATUS_Y + 38, line, Font_11x18,
                        ILI9341_WHITE, ILI9341_BLACK);
 
    snprintf(line, sizeof(line), "Porta:   %s", st->door ? "ABERTA" : "FECHADA");
    ILI9341_WriteString(8, IHM_STATUS_Y + 60, line, Font_11x18,
                        st->door ? ILI9341_RED : ILI9341_GREEN, ILI9341_BLACK);
}

void IHM_Process(void)
{
    if (!s_ready || !s_showing_event) return;

    /* subtração unsigned -> segura contra wraparound do tick de 32 bits */
    if ((HAL_GetTick() - s_event_ts) >= IHM_EVENT_TIMEOUT_MS)
        IHM_ShowIdle();
}