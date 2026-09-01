#include "wheel/pulse_gate.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Internal pulse-gated interface modes and hold intervals. */
enum {
    INTERFACE_MODE_XBOX_GIP = 6,         /**< Xbox GIP interface mode. */
    INTERFACE_MODE_PLAYSTATION_4 = 7,    /**< PlayStation 4 interface mode. */
    INTERFACE_MODE_AUXILIARY_PULSE = 10, /**< Auxiliary pulse interface mode. */
    XBOX_PULSE_HOLD_MS = 90,             /**< Xbox pulse hold interval in milliseconds. */
    PLAYSTATION_PULSE_HOLD_MS = 15, /**< PlayStation and auxiliary hold interval in milliseconds. */
};

/**
 * @brief Clears the interface pulse deadlines.
 *
 * Returns both independent interface gates to their initial zero deadline.
 *
 * @param[out] gate Pulse timing state to initialize.
 */
void wheel_pulse_gate_init(WheelPulseGate *gate) {
    gate->deadlines_ms[0] = 0;
    gate->deadlines_ms[1] = 0;
}

/**
 * @brief Applies the attached-wheel interface pulse timing policy.
 *
 * Publishes pulses immediately on direct interfaces. Xbox and PlayStation pulses are accepted only
 * after their independent 90 ms and 15 ms hold intervals. The auxiliary-pulse interface shares the
 * 15 ms gate. A nonzero accepted pulse starts the next interval.
 *
 * @param[in,out] gate Independent Xbox and PlayStation pulse deadlines.
 * @param[in] interface_mode Active wheel interface mode.
 * @param[in] now_ms Current monotonic millisecond count.
 * @param[in] pulse_flags Positive and negative pulse flags.
 * @return True when the pulse flags may update logical motion counters.
 */
bool wheel_pulse_gate_ready(WheelPulseGate *gate, uint8_t interface_mode, uint32_t now_ms,
                            uint8_t pulse_flags) {
    uint8_t deadline_index;
    uint32_t hold_ms;
    if (interface_mode == INTERFACE_MODE_XBOX_GIP) {
        deadline_index = 0;
        hold_ms = XBOX_PULSE_HOLD_MS;
    } else if (interface_mode == INTERFACE_MODE_PLAYSTATION_4 ||
               interface_mode == INTERFACE_MODE_AUXILIARY_PULSE) {
        deadline_index = 1;
        hold_ms = PLAYSTATION_PULSE_HOLD_MS;
    } else {
        return true;
    }

    if (now_ms <= gate->deadlines_ms[deadline_index]) {
        return false;
    }
    if (pulse_flags != 0) {
        gate->deadlines_ms[deadline_index] = now_ms + hold_ms;
    }
    return true;
}
