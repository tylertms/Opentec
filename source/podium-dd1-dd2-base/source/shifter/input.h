#ifndef OPENTEC_BASE_SHIFTER_INPUT_H
#define OPENTEC_BASE_SHIFTER_INPUT_H

#include <stdbool.h>

typedef enum {
    SHIFTER_INPUT_H_PATTERN = 1,
    SHIFTER_INPUT_SEQUENTIAL = 2,
} ShifterInputMode;

typedef struct {
    ShifterInputMode primary_mode;
    ShifterInputMode secondary_mode;
    bool primary_transition;
    bool secondary_transition;
} ShifterInputState;

#endif
