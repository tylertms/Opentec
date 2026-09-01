#ifndef OPENTEC_BASE_USB_FANATEC_ENCODER_H
#define OPENTEC_BASE_USB_FANATEC_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/fanatec_input.h"

/** @brief Retained Fanatec rotary encoder position and presentation phase. */
typedef struct {
    uint32_t deadline_ms;           /**< Deadline for the current encoder presentation phase. */
    uint32_t secondary_deadline_ms; /**< Deadline for the secondary encoder phase. */
    uint8_t position;               /**< Current unsigned rotary position. */
    bool quiet_phase;           /**< True while direction buttons are suppressed between steps. */
    bool secondary_quiet_phase; /**< True while secondary direction output is suppressed. */
} FanatecEncoder;

/**
 * @brief Initializes Fanatec encoder report presentation.
 *
 * Clears the persistent dial position, transition deadline, and quiet phase.
 *
 * @param[out] encoder Encoder presentation state to initialize.
 */
void fanatec_encoder_init(FanatecEncoder *encoder);

/**
 * @brief Advances the Fanatec encoder report presentation.
 *
 * Presents queued motion as alternating direction and quiet phases and updates the persistent dial
 * position when a direction phase is consumed.
 *
 * @param[in,out] encoder Persistent encoder presentation state.
 * @param[in] pending_direction Sign of the queued attached-wheel motion.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] input Fanatec input state that receives the dial position and direction buttons.
 * @return True when one queued motion step must be consumed; otherwise false.
 */
bool fanatec_encoder_update(FanatecEncoder *encoder, int8_t pending_direction, uint32_t now_ms,
                            fanatec_input_state *input);

/**
 * @brief Advances secondary Fanatec encoder direction presentation.
 *
 * @param[in,out] encoder Persistent encoder presentation state.
 * @param[in] pending_direction Sign of the queued secondary motion.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] input Fanatec input state receiving secondary direction buttons.
 * @return True when one queued secondary motion step must be consumed; otherwise false.
 */
bool fanatec_encoder_update_secondary(FanatecEncoder *encoder, int8_t pending_direction,
                                      uint32_t now_ms, fanatec_input_state *input);

#endif
