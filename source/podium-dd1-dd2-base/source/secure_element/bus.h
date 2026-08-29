#ifndef OPENTEC_BASE_A71CH_BUS_H
#define OPENTEC_BASE_A71CH_BUS_H

#include <stdbool.h>
#include <stdint.h>

#include "secure_element/a71ch.h"

bool a71ch_bus_start(A71chCommand command, uint8_t *response);
bool a71ch_bus_start_frame_write(const A71chAuthenticationFrame *frame);
bool a71ch_bus_start_frame_read(const A71chAuthenticationFrame *frame, uint8_t *response);

#endif
