#ifndef MODULES_STATUS_POLL_H
#define MODULES_STATUS_POLL_H

/* Timer periódico do lado DOOR: dispara o auto-anúncio de status
 * (temperatura/umidade/porta) em intervalos regulares, com um deslocamento
 * inicial (jitter) derivado de MY_ADDR pra reduzir colisão no rádio quando
 * vários módulos ligam ao mesmo tempo. Chamar apenas em builds
 * MODULE_TRANSMITTER — o HUB não usa esse módulo, só escuta
 * EVENT_STATUS_UPDATE. */
void StatusPoll_Process(void);

#endif /* MODULES_STATUS_POLL_H */
