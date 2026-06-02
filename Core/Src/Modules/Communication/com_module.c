#include "com_module.h"
#include "cc1101.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;

/* Endereço deste nó — salvo na init para uso no log do Send */
static COM_RxCallback_t s_rx_callback = NULL;
static uint8_t s_my_addr = 0x00;
extern volatile uint8_t s_rx_ready;

/* ---------------------------------------------------------- */
/* Funções internas                                           */
/* ---------------------------------------------------------- */

static void _define_source(uint8_t my_addr)
{
    TI_write_reg(CCxxx0_ADDR, my_addr);
    printf("[CC1101] ORIGIN=0x%02X\r\n", my_addr);
}

static void _check_version(void)
{
    uint8_t version = TI_read_status(CCxxx0_VERSION);
    printf("[CC1101] VERSION=0x%02X\r\n", version);
}

static void _start_rx(void)
{
    TI_strobe(CCxxx0_SIDLE);
    HAL_Delay(10);
    TI_strobe(CCxxx0_SFRX);
    HAL_Delay(10);
    TI_strobe(CCxxx0_SRX);
    HAL_Delay(500);
}

static void _check_status(void)
{
    /*
     * MARCSTATE — estado interno do CC1101 (CCxxx0_MARCSTATE & 0x1F)
     *
     * 0x01  IDLE             Parado, não faz nada
     * 0x0D  RX               Escutando, esperando pacote
     * 0x0E  RX_END           Terminou de receber
     * 0x11  RXFIFO_OVERFLOW  FIFO cheio — recuperar com SIDLE+SFRX+SRX
     * 0x13  TX               Transmitindo
     */
    uint8_t marc = TI_read_status(CCxxx0_MARCSTATE) & 0x1F;
    printf("[CC1101] STATE=0x%02X\r\n", marc);
}

static void _log_packet(const char *dir, uint8_t src, uint8_t dst, uint8_t event, const uint8_t *payload, uint8_t payload_len)
{
    printf("[CC1101] [%s] 0x%02X -> 0x%02X event=0x%02X | %u byte(s):", dir, src, dst, event, payload_len);
    for (uint8_t i = 0; i < payload_len; i++)
        printf(" %02X", payload[i]);
    printf("\r\n");
}

/* ---------------------------------------------------------- */
/* API pública                                                */
/* ---------------------------------------------------------- */

void COM_Module_Init(uint8_t my_addr)
{
    s_my_addr = my_addr;
    Power_up_reset(&hspi1, NSS_CC1101_GPIO_Port, NSS_CC1101_Pin);
    TI_init(&hspi1, NSS_CC1101_GPIO_Port, NSS_CC1101_Pin);
    _define_source(my_addr);
    _check_version();
    _start_rx();
    _check_status();
}

void COM_Module_SetAddr(uint8_t new_addr)
{
    s_my_addr = new_addr;
    TI_strobe(CCxxx0_SIDLE);
    TI_write_reg(CCxxx0_ADDR, new_addr);
    TI_strobe(CCxxx0_SFRX);
    TI_strobe(CCxxx0_SRX);
}

void COM_Module_Send(const COM_TxPacket_t *pkt)
{
    uint8_t tx_buf[64];
    tx_buf[0] = pkt->dst;
    tx_buf[1] = pkt->event;
    tx_buf[2] = s_my_addr;
    memcpy(&tx_buf[3], pkt->payload, pkt->payload_len);
    TI_send_packet(tx_buf, pkt->payload_len + 3);

    uint32_t deadline = HAL_GetTick() + COM_TIMEOUT;
    while (HAL_GPIO_ReadPin(INT_CC1101_GPIO_Port, INT_CC1101_Pin) == GPIO_PIN_RESET && HAL_GetTick() < deadline);   /* aguarda GDO0 subir (TX ativo)    */
    while (HAL_GPIO_ReadPin(INT_CC1101_GPIO_Port, INT_CC1101_Pin) == GPIO_PIN_SET && HAL_GetTick() < deadline);   /* aguarda GDO0 cair (TX concluído) */

    s_rx_ready = 0;
    _log_packet("TX", s_my_addr, pkt->dst, pkt->event, pkt->payload, pkt->payload_len);
}

void COM_Module_Process(void)
{
    if (!s_rx_ready) return;
    s_rx_ready = 0;

    uint8_t buf[64];
    uint8_t len = sizeof(buf);

    if (TI_receive_packet(buf, &len) && s_rx_callback && len >= 3) {
        uint8_t event = buf[1];
        uint8_t src = buf[2];
        uint8_t payload_len = len - 3;

        _log_packet("RX", src, s_my_addr, event, &buf[3], payload_len);

        s_rx_callback(event, src, &buf[3], payload_len);
    }

    // Garante retorno ao RX: necessário após RX (RXOFF=IDLE)
    TI_strobe(CCxxx0_SIDLE);
    TI_strobe(CCxxx0_SFRX);
    TI_strobe(CCxxx0_SRX);
}

void COM_Module_SetRxCallback(COM_RxCallback_t cb)
{
    s_rx_callback = cb;
}