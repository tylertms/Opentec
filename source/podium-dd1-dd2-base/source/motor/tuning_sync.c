#include "motor/tuning_sync.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Returns the dirty-mask bit for one motor tuning parameter.
 *
 * Converts the zero-based parameter index to its sixteen-bit synchronization mask.
 *
 * @param[in] parameter Zero-based motor tuning parameter index.
 * @return Bit selecting the parameter.
 */
static uint16_t parameter_bit(uint8_t parameter) { return UINT16_C(1) << parameter; }

/**
 * @brief Compares two complete motor parameter writes.
 *
 * Requires matching addresses, transfer widths, and both data bytes.
 *
 * @param[in] left First motor parameter write.
 * @param[in] right Second motor parameter write.
 * @return True when both writes are identical.
 */
static bool writes_equal(const MotorParameterWrite *left, const MotorParameterWrite *right) {
    return left->address == right->address && left->length == right->length &&
           left->data[0] == right->data[0] && left->data[1] == right->data[1];
}

/**
 * @brief Refreshes the desired motor tuning parameter set.
 *
 * Re-encodes every supported parameter and marks entries dirty when their desired write changed or
 * has not previously been initialized.
 *
 * @param[in,out] sync Motor tuning synchronization state to refresh.
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime motor tuning context.
 */
void motor_tuning_sync_refresh(MotorTuningSync *sync, const TuningProfile *profile,
                               const MotorTuningContext *context) {
    for (uint8_t parameter = 0; parameter < MOTOR_TUNING_PARAMETER_COUNT; parameter++) {
        uint16_t bit = parameter_bit(parameter);
        MotorParameterWrite *desired = &sync->desired[parameter];
        uint8_t old_address = desired->address;
        uint8_t old_length = desired->length;
        uint8_t old_data_0 = desired->data[0];
        uint8_t old_data_1 = desired->data[1];
        motor_tuning_parameter_encode((MotorTuningParameter)parameter, profile, context, desired);
        if ((sync->valid_parameters & bit) == 0 || old_address != desired->address ||
            old_length != desired->length || old_data_0 != desired->data[0] ||
            old_data_1 != desired->data[1]) {
            sync->valid_parameters |= bit;
            sync->dirty_parameters |= bit;
        }
    }
}

/**
 * @brief Initializes motor tuning synchronization from the active settings.
 *
 * Clears transfer progress and marks the complete initial parameter set for transmission.
 *
 * @param[out] sync Motor tuning synchronization state to initialize.
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime motor tuning context.
 */
void motor_tuning_sync_init(MotorTuningSync *sync, const TuningProfile *profile,
                            const MotorTuningContext *context) {
    sync->valid_parameters = 0;
    sync->dirty_parameters = 0;
    sync->next_parameter = 0;
    sync->in_flight_parameter = 0;
    sync->in_flight = false;
    motor_tuning_sync_refresh(sync, profile, context);
}

/**
 * @brief Takes the next dirty motor parameter write.
 *
 * Scans from the rotating cursor, records the selected write as in flight, and blocks additional
 * writes until its completion is reported.
 *
 * @param[in,out] sync Motor tuning synchronization state.
 * @param[out] write Next motor parameter write to transmit.
 * @return True when a write was selected.
 */
bool motor_tuning_sync_next(MotorTuningSync *sync, MotorParameterWrite *write) {
    if (sync->in_flight) {
        return false;
    }

    for (uint8_t offset = 0; offset < MOTOR_TUNING_PARAMETER_COUNT; offset++) {
        uint8_t parameter = (uint8_t)(sync->next_parameter + offset);
        if (parameter >= MOTOR_TUNING_PARAMETER_COUNT) {
            parameter -= MOTOR_TUNING_PARAMETER_COUNT;
        }
        if ((sync->dirty_parameters & parameter_bit(parameter)) == 0) {
            continue;
        }

        sync->in_flight_parameter = parameter;
        sync->in_flight_write = sync->desired[parameter];
        sync->in_flight = true;
        *write = sync->in_flight_write;
        return true;
    }
    return false;
}

/**
 * @brief Completes the current motor parameter write.
 *
 * Clears the dirty bit only after a successful transfer whose value still matches the current
 * desired write. Failed or superseded writes remain pending for retry.
 *
 * @param[in,out] sync Motor tuning synchronization state.
 * @param[in] succeeded True when the motor parameter transfer completed successfully.
 */
void motor_tuning_sync_complete(MotorTuningSync *sync, bool succeeded) {
    if (!sync->in_flight) {
        return;
    }

    uint8_t parameter = sync->in_flight_parameter;
    if (succeeded && writes_equal(&sync->desired[parameter], &sync->in_flight_write)) {
        sync->dirty_parameters &= (uint16_t)~parameter_bit(parameter);
        sync->next_parameter = parameter + 1;
        if (sync->next_parameter == MOTOR_TUNING_PARAMETER_COUNT) {
            sync->next_parameter = 0;
        }
    } else {
        sync->next_parameter = parameter;
    }
    sync->in_flight = false;
}

/**
 * @brief Reports whether motor tuning synchronization has outstanding work.
 *
 * Includes both a transfer awaiting completion and any dirty parameter waiting to start.
 *
 * @param[in] sync Motor tuning synchronization state.
 * @return True while any motor tuning write remains outstanding.
 */
bool motor_tuning_sync_pending(const MotorTuningSync *sync) {
    return sync->in_flight || sync->dirty_parameters != 0;
}
