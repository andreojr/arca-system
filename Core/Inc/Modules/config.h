#ifndef CONFIG_H
#define CONFIG_H

/* Endereços dos módulos */
#define ADDR_BROADCAST   0x00
#define ADDR_CONTROLLER  0x01
#define ADDR_ROOM_1_EXT  0x11
#define ADDR_ROOM_1_INT  0x12
#define ADDR_ROOM_2_EXT  0x21
#define ADDR_ROOM_2_INT  0x22

/* Eventos do protocolo */
#define EVENT_AUTHORIZE         0x01
#define EVENT_ACCESS_GRANTED    0x02
#define EVENT_ACCESS_DENIED     0x03
#define EVENT_STATUS_REQUEST    0x04
#define EVENT_STATUS_RESPONSE   0x05
#define EVENT_ACCESS_CONFIRMED  0x06

#ifndef MY_ADDR
#define MY_ADDR 0x00
#endif

#if MY_ADDR == ADDR_BROADCAST
    #warning "MY_ADDR nao definido — modulo desconhecido"
#elif MY_ADDR == ADDR_CONTROLLER
    #define MODULE_CONTROLLER
#else
    #define MODULE_TRANSMITTER
#endif

#endif /* CONFIG_H */