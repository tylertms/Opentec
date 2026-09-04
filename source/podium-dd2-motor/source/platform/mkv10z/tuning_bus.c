#include "platform/tuning_bus.h"

#include <fsl_i2c.h>
#include <string.h>

/**
 * @brief Internal state of one motor parameter-bus transaction.
 */
typedef enum {
    kMotorBusIdle,         /**< No receive or transmit payload is active. */
    kMotorBusReceiving,    /**< A parameter write payload is being received. */
    kMotorBusTransmitting, /**< A parameter response payload is being transmitted. */
} MotorBusState;

/** @brief NXP I2C slave-transfer handle for the parameter bus. */
static i2c_slave_handle_t motor_bus_handle;
/** @brief Parameter bank exposed through the I2C service. */
static MotorParameterBank *motor_bus_parameters;
/** @brief Callback invoked after an accepted live-control parameter change. */
static MotorParameterChangedHandler motor_bus_changed_handler;
/** @brief Context passed to the parameter-change callback. */
static void *motor_bus_context;
/** @brief Receive buffer for one parameter request. */
static uint8_t motor_bus_receive[MOTOR_PARAMETER_REQUEST_SIZE];
/** @brief Current internal parameter-bus transaction state. */
static MotorBusState motor_bus_state;
/** @brief True while an I2C transaction requires service or completion. */
static volatile bool motor_bus_active;
/** @brief True while the first receive event after a start is the extended header. */
static bool motor_bus_extended_header_pending;
/** @brief Number of service ticks elapsed during the active transaction. */
static uint16_t motor_bus_active_ticks;
/** @brief Number of request bytes observed by the receive-event callback. */
static uint8_t motor_bus_receive_size;

/**
 * @brief Restores the official idle parameter-selection buffer.
 *
 * An invalid index prevents a read until the controller supplies a fresh selection.
 */
static void motor_bus_receive_reset(void) {
    memset(motor_bus_receive, 0, sizeof(motor_bus_receive));
    motor_bus_receive[0] = UINT8_MAX;
    motor_bus_receive_size = 0U;
}

/**
 * @brief Prepares the selected parameter for one I2C read transaction.
 *
 * A valid selection publishes the live five-byte value and width fields, while an invalid index
 * suppresses data. The SDK consumes the parameter entry directly so updates between transmitted
 * bytes remain visible on the bus. The receive buffer remains intact across a repeated start, so
 * its selector can select the response without a separate latch.
 *
 * @param[in] parameters Live motor parameter bank.
 * @param[out] transfer Active NXP SDK slave transfer descriptor.
 */
static void motor_bus_transmit_prepare(const MotorParameterBank *parameters,
                                       i2c_slave_transfer_t *transfer) {
    const uint8_t selected_parameter = motor_bus_receive[0];
    if (selected_parameter >= MOTOR_PARAMETER_COUNT) {
        transfer->data = NULL;
        transfer->dataSize = 0U;
        return;
    }
    transfer->data = (uint8_t *)&parameters->entries[selected_parameter].value;
    transfer->dataSize = MOTOR_PARAMETER_RESPONSE_SIZE;
    motor_bus_state = kMotorBusTransmitting;
}

/**
 * @brief Applies one completed I2C parameter write transaction.
 *
 * Accepted live-control writes notify the runtime after updating the parameter bank.
 *
 * @param[in,out] parameters Live motor parameter bank.
 * @param[in] transfer Completed NXP SDK slave transfer descriptor.
 */
static void motor_bus_receive_complete(MotorParameterBank *parameters,
                                       const i2c_slave_transfer_t *transfer) {
    bool control_settings_changed;
    size_t transferred = motor_bus_receive_size == 0U ? transfer->transferredCount
                                                       : motor_bus_receive_size;
    uint8_t received_size =
        transferred > sizeof(motor_bus_receive) ? sizeof(motor_bus_receive) : (uint8_t)transferred;
    if (motor_parameter_request_apply(parameters, motor_bus_receive, received_size,
                                      &control_settings_changed) &&
        control_settings_changed && motor_bus_changed_handler != NULL) {
        motor_bus_changed_handler(motor_bus_context);
    }
}

/**
 * @brief Restores an I2C transfer descriptor to the idle state.
 *
 * Transaction state, data pointers, and transfer counters are cleared. The receive buffer and its
 * byte count remain untouched so an in-progress request can continue after a repeated start.
 *
 * @param[out] transfer Active NXP SDK slave transfer descriptor.
 */
static void motor_bus_transfer_reset(i2c_slave_transfer_t *transfer) {
    motor_bus_state = kMotorBusIdle;
    motor_bus_extended_header_pending = false;
    transfer->data = NULL;
    transfer->dataSize = 0U;
    transfer->transferredCount = 0U;
}

/**
 * @brief Handles NXP SDK slave-transfer events for the motor parameter bank.
 *
 * Receive transactions accept an index plus up to four value bytes. Transmit transactions return
 * the selected value and its declared width.
 *
 * @param[in] base Active I2C peripheral instance.
 * @param[in,out] transfer NXP SDK transfer descriptor for the current event.
 * @param[in] user_data Parameter bank installed as the callback context.
 */
static void motor_bus_transfer_callback(I2C_Type *base, i2c_slave_transfer_t *transfer,
                                        void *user_data) {
    MotorParameterBank *parameters = user_data;
    switch (transfer->event) {
    case kI2C_SlaveStartEvent:
        if (!motor_bus_active) {
            motor_bus_transfer_reset(transfer);
            motor_bus_receive_reset();
        }
        motor_bus_extended_header_pending = true;
        motor_bus_active = true;
        motor_bus_active_ticks = 0U;
        break;
    case kI2C_SlaveAddressMatchEvent:
        motor_bus_extended_header_pending = false;
        if ((I2C_SlaveGetStatusFlags(base) & (uint32_t)kI2C_TransferDirectionFlag) != 0U) {
            transfer->transferredCount = 0U;
            motor_bus_transmit_prepare(parameters, transfer);
        } else if (motor_bus_state == kMotorBusIdle) {
            transfer->data = motor_bus_receive;
            transfer->dataSize = sizeof(motor_bus_receive);
            motor_bus_state = kMotorBusReceiving;
        }
        motor_bus_active = true;
        break;
    case kI2C_SlaveGenaralcallEvent:
        motor_bus_extended_header_pending = false;
        motor_bus_transfer_reset(transfer);
        motor_bus_receive_reset();
        motor_bus_active = true;
        break;
    case kI2C_SlaveReceiveEvent:
        if (motor_bus_extended_header_pending) {
            motor_bus_extended_header_pending = false;
        } else if (motor_bus_state == kMotorBusIdle) {
            transfer->data = motor_bus_receive;
            transfer->dataSize = sizeof(motor_bus_receive);
            motor_bus_state = kMotorBusReceiving;
        } else if (motor_bus_state == kMotorBusReceiving) {
            motor_bus_receive_size = (uint8_t)transfer->transferredCount;
        }
        break;
    case kI2C_SlaveTransmitEvent:
        if (motor_bus_state == kMotorBusTransmitting && transfer->dataSize == 0U) {
            motor_bus_transfer_reset(transfer);
            motor_bus_receive_reset();
        } else {
            motor_bus_transmit_prepare(parameters, transfer);
        }
        break;
    case kI2C_SlaveCompletionEvent: {
        if (motor_bus_state == kMotorBusReceiving &&
            transfer->completionStatus == kStatus_Success) {
            motor_bus_receive_complete(parameters, transfer);
        }
        const bool completed = transfer->completionStatus == kStatus_Success;
        motor_bus_transfer_reset(transfer);
        motor_bus_active = !completed;
        if (completed) {
            motor_bus_active_ticks = 0U;
        } else {
            motor_bus_receive_reset();
            motor_bus_handle.isBusy = true;
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @brief Configures and starts the NXP SDK I2C slave transaction engine.
 *
 * Address, timing, filtering, callback state, and the nonblocking event mask are restored.
 */
static void motor_bus_hardware_initialize(void) {
    i2c_slave_config_t config;

    I2C_SlaveGetDefaultConfig(&config);
    config.slaveAddress = 0x78U;
    I2C_SlaveInit(I2C0, &config, CLOCK_GetFreq(kCLOCK_BusClk));

    I2C0->F = 0x27U;

    I2C_SlaveTransferCreateHandle(I2C0, &motor_bus_handle, motor_bus_transfer_callback,
                                  motor_bus_parameters);
    I2C_SlaveTransferNonBlocking(I2C0, &motor_bus_handle, kI2C_SlaveAllEvents);
    I2C0->FLT = (I2C0->FLT & 0xa0U) | 4U;
}

void motor_bus_initialize(MotorParameterBank *parameters,
                          MotorParameterChangedHandler changed_handler, void *context) {
    motor_bus_parameters = parameters;
    motor_bus_changed_handler = changed_handler;
    motor_bus_context = context;
    motor_bus_state = kMotorBusIdle;
    motor_bus_active = false;
    motor_bus_extended_header_pending = false;
    motor_bus_active_ticks = 0U;
    motor_bus_receive_reset();
    motor_bus_hardware_initialize();
}

void motor_bus_service(void) {
    if (!motor_bus_active) {
        motor_bus_active_ticks = 0U;
        return;
    }

    ++motor_bus_active_ticks;
    if (motor_bus_active_ticks <= 9U) {
        return;
    }

    I2C_SlaveTransferAbort(I2C0, &motor_bus_handle);
    I2C_SlaveDeinit(I2C0);
    motor_bus_state = kMotorBusIdle;
    motor_bus_active = false;
    motor_bus_extended_header_pending = false;
    motor_bus_active_ticks = 0U;
    motor_bus_receive_reset();
    motor_bus_hardware_initialize();
}

/**
 * @brief Dispatches the official I2C0 vector through the NXP SDK slave driver.
 *
 * The SDK advances the active transaction and invokes the motor parameter callback as needed.
 */
void I2C0_IRQHandler(void) { I2C_SlaveTransferHandleIRQ(I2C0, &motor_bus_handle); }
