#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_INPUT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Sizes and capacities used by force-feedback script input packets.
 *
 * Sample packets contain ten records in a 64-byte packet, and live input packets contain three
 * input slots.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_PACKET_SIZE = 64,         /**< Required vendor-HID packet length. */
    FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT = 512,       /**< Number of entries in the sample table. */
    FORCE_FEEDBACK_SCRIPT_SAMPLE_UPDATE_COUNT = 10, /**< Number of records in a sample packet. */
    FORCE_FEEDBACK_SCRIPT_INPUT_SLOT_COUNT = 3,     /**< Number of live input slots. */
};

/**
 * @brief Status byte used by force-feedback script input packets and slots.
 *
 * The values identify position input, active input, ready input, and an unused slot record.
 */
typedef uint8_t ForceFeedbackScriptInputStatus;

/**
 * @brief Force-feedback script input status values.
 *
 * Position is the packet and slot status used for position-mode input; active and ready identify
 * packets that carry live script input; unused preserves an existing slot record.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_INPUT_POSITION = 0,       /**< Position-input status. */
    FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE = 1,         /**< Active live-input status. */
    FORCE_FEEDBACK_SCRIPT_INPUT_READY = 0xf0,       /**< Ready live-input status. */
    FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED = UINT8_MAX, /**< Unused slot-record status. */
};

/**
 * @brief Force-feedback script sample table.
 *
 * Each entry stores one raw 32-bit script value. Initialization marks all entries with
 * UINT32_MAX.
 */
typedef struct {
    uint32_t
        values[FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT]; /**< Raw values indexed by sample number. */
} ForceFeedbackScriptSamples;

/**
 * @brief One live force-feedback script input slot.
 *
 * An active or ready packet replaces a slot when its record status is not
 * FORCE_FEEDBACK_SCRIPT_INPUT_UNUSED; unused records retain the previous slot data.
 */
typedef struct {
    ForceFeedbackScriptInputStatus status; /**< Slot status from the most recent update. */
    uint32_t value;                        /**< Raw value supplied for the slot. */
    uint32_t duration;                     /**< Duration supplied for the slot value. */
} ForceFeedbackScriptInputSlot;

/**
 * @brief Live force-feedback script input state.
 *
 * Active and ready packets update the deadline, sample count, and selected slots. Position-status
 * slots also update position_value.
 */
typedef struct {
    ForceFeedbackScriptInputStatus status; /**< Most recently received packet status. */
    uint32_t deadline;                     /**< Absolute script-sample deadline. */
    uint16_t sample_count;                 /**< Host-requested interval between live-input ticks. */
    ForceFeedbackScriptInputSlot
        slots[FORCE_FEEDBACK_SCRIPT_INPUT_SLOT_COUNT]; /**< Live slot values. */
    uint32_t position_value; /**< Raw value from the most recent position slot. */
} ForceFeedbackScriptInputs;

/**
 * @brief Initialize a force-feedback script sample table.
 *
 * Clears the sample table to its unused-entry representation. A null samples pointer is ignored.
 *
 * @param[out] samples Sample table to initialize.
 */
void force_feedback_script_samples_init(ForceFeedbackScriptSamples *samples);

/**
 * @brief Apply one force-feedback script sample packet.
 *
 * Requires a 64-byte packet with opcode 0x0b. All ten sample indexes are validated before any
 * selected values are written; indexes 0 through 511 are written and UINT16_MAX skips a record.
 *
 * @param[in,out] samples Sample table to update.
 * @param[in] packet Vendor-HID packet containing ten sample records.
 * @param[in] length Number of bytes available in packet.
 * @return true when packet validation and all sample updates succeed; otherwise false.
 */
bool force_feedback_script_samples_apply(ForceFeedbackScriptSamples *samples, const uint8_t *packet,
                                         size_t length);

/**
 * @brief Initialize live force-feedback script input state.
 *
 * Clears the deadline, sample count, position value, slot values, and durations, selects position
 * status, and marks every input slot unused. A null inputs pointer is ignored.
 *
 * @param[out] inputs Live input state to initialize.
 */
void force_feedback_script_inputs_init(ForceFeedbackScriptInputs *inputs);

/**
 * @brief Apply one live force-feedback script input packet.
 *
 * Requires a 64-byte packet with opcode 0x0e and always stores its status. Active and ready status
 * update the absolute deadline from current_sample_count plus the packet deadline delta, store the
 * host-requested sample interval, and replace each non-unused slot; a position slot also replaces
 * position_value. Other statuses leave those fields unchanged.
 *
 * @param[in,out] inputs Live input state to update.
 * @param[in] current_sample_count Current script sample counter used as deadline base.
 * @param[in] packet Vendor-HID packet containing live input fields.
 * @param[in] length Number of bytes available in packet.
 * @return true when packet opcode and length are valid; otherwise false.
 */
bool force_feedback_script_inputs_apply(ForceFeedbackScriptInputs *inputs,
                                        uint32_t current_sample_count, const uint8_t *packet,
                                        size_t length);

#endif
