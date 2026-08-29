#ifndef OPENTEC_BASE_I2C_PROBE_BUS_H
#define OPENTEC_BASE_I2C_PROBE_BUS_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c/probe.h"

bool i2c_probe_bus_start(I2cProbeCommand command, uint8_t *response);

#endif
