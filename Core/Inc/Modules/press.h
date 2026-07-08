#ifndef MODULES_PRESS_H
#define MODULES_PRESS_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* Margem acima da leitura de repouso (calibrada em Press_Init) que conta como
 * pressionado. Absoluto em vez de percentual porque a faixa útil do FSR é
 * pequena; ajuste se o sensor for trocado por um de sensibilidade diferente. */
#define PRESS_MARGIN    400u
#define DEBOUNCE_MS     50u

void    Press_Init(void);
bool    Press_IsDoorPressed(void);

#endif /* MODULES_PRESS_H */
