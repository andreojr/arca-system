#ifndef MODULES_PRESS_H
#define MODULES_PRESS_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define THRESHOLD_DOOR  2000u
#define DEBOUNCE_MS     50u

bool    Press_IsDoorPressed(void);

#endif /* MODULES_PRESS_H */
