#include "ihm.h"
#include "ili9341.h"
#include "config.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include "icons.h"
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
 *   |  SALA n  ENTRADA / SAIDA         |   linha de evento  ─┐
 *   |  UID: XX XX XX XX                |                     │ corpo
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
 *
 * Redesenho: o bloco de status é redesenhado por diff — só o campo que
 * mudou é limpo e reescrito. O ícone de cada linha só é redesenhado
 * quando a categoria muda (temp: frio/normal/quente; porta: aberta/
 * fechada); a umidade tem ícone fixo, redesenhado uma vez no full_redraw.
 */

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

/* Ícones */
extern const uint16_t temp_quente[];
extern const uint16_t temp_padrao[];
extern const uint16_t temp_frio[];
extern const uint16_t umidade[];
extern const uint16_t porta_fechada[];
extern const uint16_t porta_aberta[];

typedef enum {
    IHM_TEMP_CAT_FRIO = 0,
    IHM_TEMP_CAT_NORMAL,
    IHM_TEMP_CAT_QUENTE
} IHM_TempCat_t;

static uint8_t  s_ready         = 0;        /* display inicializado            */
static uint8_t  s_showing_event = 0;        /* há evento na tela aguardando expirar */
static uint32_t s_event_ts      = 0;        /* HAL_GetTick() do último evento  */

static uint8_t      s_has_status     = 0;   /* já recebemos algum STATUS_UPDATE */
static IHM_Status_t s_last_status;          /* último status desenhado, p/ diff */
static uint32_t     s_bar_start_tick = 0;   /* tick do último STATUS_UPDATE     */
static uint16_t     s_bar_filled_px  = 0;   /* largura já pintada da barra      */
static IHM_TempCat_t s_last_temp_cat = IHM_TEMP_CAT_NORMAL; /* p/ diff */

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

/* Limpa só a coluna de texto de uma linha com ícone */
static void _clear_row_text(uint16_t y)
{
    ILI9341_FillRectangle(IHM_TEXT_X, y, ILI9341_WIDTH - IHM_TEXT_X - 8,
                          IHM_ICON_SIZE, ILI9341_BLACK);
}

/* Limpa ícone + texto de uma linha inteira */
static void _clear_row_full(uint16_t y)
{
    ILI9341_FillRectangle(0, y, ILI9341_WIDTH, IHM_ICON_SIZE, ILI9341_BLACK);
}

static void _bar_reset(void)
{
    ILI9341_FillRectangle(IHM_BAR_MARGIN, IHM_BAR_Y, IHM_BAR_W, IHM_BAR_H, ILI9341_BLACK);
    s_bar_filled_px  = 0;
    s_bar_start_tick = HAL_GetTick();
}

/* Pinta só a fatia nova da barra desde o último avanço */
static void _bar_advance(void)
{
    uint32_t elapsed    = HAL_GetTick() - s_bar_start_tick;
    uint32_t target_px  = (elapsed >= STATUS_UPDATE_INTERVAL_MS)
                         ? IHM_BAR_W
                         : (elapsed * IHM_BAR_W) / STATUS_UPDATE_INTERVAL_MS;

    if (target_px <= s_bar_filled_px) return;

    ILI9341_FillRectangle(IHM_BAR_MARGIN + s_bar_filled_px, IHM_BAR_Y,
                          target_px - s_bar_filled_px, IHM_BAR_H, ILI9341_GREEN);
    s_bar_filled_px = target_px;
}

static void _draw_header(void)
{
    ILI9341_FillScreen(ILI9341_BLACK);
    ILI9341_FillRectangle(0, 0, ILI9341_WIDTH, IHM_HEADER_H, ILI9341_BLUE);
    ILI9341_WriteString(70, 12, "A.R.C.A.", Font_11x18, ILI9341_WHITE, ILI9341_BLUE);
}

static IHM_TempCat_t _temp_category(uint8_t temp)
{
    if (temp >= IHM_TEMP_LIMIAR_QUENTE) return IHM_TEMP_CAT_QUENTE;
    if (temp <= IHM_TEMP_LIMIAR_FRIO)   return IHM_TEMP_CAT_FRIO;
    return IHM_TEMP_CAT_NORMAL;
}

static const uint16_t *_temp_icon(IHM_TempCat_t cat)
{
    switch (cat) {
        case IHM_TEMP_CAT_QUENTE: return temp_quente;
        case IHM_TEMP_CAT_FRIO:   return temp_frio;
        default:                  return temp_padrao;
    }
}

static uint16_t _temp_color(IHM_TempCat_t cat)
{
    switch (cat) {
        case IHM_TEMP_CAT_QUENTE: return ILI9341_RED;
        case IHM_TEMP_CAT_FRIO:   return ILI9341_CYAN;
        default:                  return ILI9341_WHITE;
    }
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
                         const uint8_t *uid, uint8_t uid_len,
                         const char *timestamp)
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

    if (timestamp != NULL) {
        ILI9341_WriteString(8, IHM_BODY_Y + 40, timestamp, Font_7x10,
                            ILI9341_WHITE, ILI9341_BLACK);
    }

    _mark_event();
}

void IHM_UpdateStatus(const IHM_Status_t *st)
{
    if (!s_ready || st == NULL) return;

    char line[32];
    /* troca de origem (outra sala) invalida o cache -> redesenha tudo */
    uint8_t full_redraw = !s_has_status || st->src != s_last_status.src;

    if (full_redraw) {
        _clear_status_area();

        uint8_t room = (st->src >> 4) & 0x0F;
        uint8_t side = st->src & 0x0F;
        snprintf(line, sizeof(line), "Sala %u (%s)", room, side == 1 ? "EXT" : "INT");
        ILI9341_WriteString(8, IHM_STATUS_Y, line, Font_7x10,
                            ILI9341_WHITE, ILI9341_BLACK);
    }

    /* Temperatura — ícone só é redesenhado se a categoria mudou */
    IHM_TempCat_t cat = _temp_category(st->temp);
    uint8_t temp_icon_changed = full_redraw || cat != s_last_temp_cat;

    if (temp_icon_changed) {
        if (!full_redraw) _clear_row_full(IHM_TEMP_Y);
        ILI9341_DrawImage(IHM_ICON_X, IHM_TEMP_Y, IHM_ICON_SIZE, IHM_ICON_SIZE,
                          _temp_icon(cat));
    } else if (st->temp != s_last_status.temp) {
        _clear_row_text(IHM_TEMP_Y);
    }
    if (temp_icon_changed || st->temp != s_last_status.temp) {
        snprintf(line, sizeof(line), "Temp:    %u C", st->temp);
        ILI9341_WriteString(IHM_TEXT_X, IHM_TEMP_Y + 8, line, Font_11x18,
                            _temp_color(cat), ILI9341_BLACK);
    }

    /* Umidade — ícone fixo, redesenhado só no full_redraw */
    if (full_redraw) {
        ILI9341_DrawImage(IHM_ICON_X, IHM_HUMIDITY_Y, IHM_ICON_SIZE, IHM_ICON_SIZE, umidade);
    }
    if (full_redraw || st->humidity != s_last_status.humidity) {
        if (!full_redraw) _clear_row_text(IHM_HUMIDITY_Y);
        snprintf(line, sizeof(line), "Umidade: %u %%", st->humidity);
        ILI9341_WriteString(IHM_TEXT_X, IHM_HUMIDITY_Y + 8, line, Font_11x18,
                            ILI9341_WHITE, ILI9341_BLACK);
    }

    /* Porta — ícone muda com o estado (aberta/fechada) */
    if (full_redraw || st->door != s_last_status.door) {
        if (!full_redraw) _clear_row_full(IHM_DOOR_Y);
        ILI9341_DrawImage(IHM_ICON_X, IHM_DOOR_Y, IHM_ICON_SIZE, IHM_ICON_SIZE,
                          st->door ? porta_aberta : porta_fechada);
        snprintf(line, sizeof(line), "Porta:   %s", st->door ? "ABERTA" : "FECHADA");
        ILI9341_WriteString(IHM_TEXT_X, IHM_DOOR_Y + 8, line, Font_11x18,
                            st->door ? ILI9341_RED : ILI9341_GREEN, ILI9341_BLACK);
    }

    s_last_status = *st;
    s_last_temp_cat = cat;
    s_has_status  = 1;
    _bar_reset();
}

void IHM_Process(void)
{
    if (!s_ready) return;

    /* subtração unsigned -> segura contra wraparound do tick de 32 bits */
    if (s_showing_event && (HAL_GetTick() - s_event_ts) >= IHM_EVENT_TIMEOUT_MS)
        IHM_ShowIdle();

    if (s_has_status)
        _bar_advance();
}