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
#define EVENT_AUTHORIZE      0x01
#define EVENT_ACCESS_GRANTED 0x02
#define EVENT_ACCESS_DENIED  0x03

#ifndef MY_ADDR
#define MY_ADDR 0x00
#endif

#endif /* CONFIG_H */