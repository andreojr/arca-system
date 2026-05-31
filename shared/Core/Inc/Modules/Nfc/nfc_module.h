#ifndef NFC_MODULE_H
#define NFC_MODULE_H

#include <stdint.h>

void NFC_Module_Init(void);
void NFC_Module_Process(void);
void NFC_Module_IRQ(void);

#endif /* NFC_MODULE_H */