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
/** @brief Transmit buffer for one parameter response. */
static uint8_t motor_bus_transmit[MOTOR_PARAMETER_RESPONSE_SIZE];
/** @brief Current internal parameter-bus transaction state. */
static MotorBusState motor_bus_state;
/** @brief True while an I2C transaction requires service or completion. */
static volatile bool motor_bus_active;
/** @brief True while the first receive event after a start is the extended header. */
static bool motor_bus_extended_header_pending;
/** @brief Number of service ticks elapsed during the active transaction. */
static uint16_t motor_bus_active_ticks;
/** @brief Number of request bytes received for the current transaction. */
static uint8_t motor_bus_receive_size;
/** @brief Parameter index selected by the preceding write transaction. */
static uint8_t motor_bus_selected_parameter;
/** @brief True when the retained parameter index is valid for a repeated-start read. */
static bool motor_bus_selected_parameter_valid;

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
 * A valid selection publishes its encoded five-byte value, while an invalid index suppresses data.
 *
 * @param[out] transfer Active NXP SDK slave transfer descriptor.
 */
static void motor_bus_transmit_prepare(i2c_slave_transfer_t *transfer) {
    MotorParameterResponse response = {0};
    const uint8_t selected_parameter =
        motor_bus_selected_parameter_valid ? motor_bus_selected_parameter : motor_bus_receive[0];
    if (!motor_parameter_read(motor_bus_parameters, selected_parameter, &response)) {
        transfer->data = NULL;
        transfer->dataSize = 0U;
        return;
    }
    motor_parameter_response_encode(&response, motor_bus_transmit);
    transfer->data = motor_bus_transmit;
    transfer->dataSize = sizeof(motor_bus_transmit);
    motor_bus_state = kMotorBusTransmitting;
}

/**
 * @brief Applies one completed I2C parameter write transaction.
 *
 * Accepted live-control writes notify the runtime after updating the parameter bank.
 *
 * @param[in] transfer Completed NXP SDK slave transfer descriptor.
 */
static void motor_bus_receive_complete(const i2c_slave_transfer_t *transfer) {
    bool control_settings_changed;
    size_t transferred =
        motor_bus_receive_size == 0U ? transfer->transferredCount : motor_bus_receive_size;
    uint8_t received_size =
        transferred > sizeof(motor_bus_receive) ? sizeof(motor_bus_receive) : (uint8_t)transferred;
    if (motor_parameter_request_apply(motor_bus_parameters, motor_bus_receive, received_size,
                                      &control_settings_changed) &&
        control_settings_changed && motor_bus_changed_handler != NULL) {
        motor_bus_changed_handler(motor_bus_context);
    }
}

/**
 * @brief Restores an I2C transfer descriptor to the idle receive state.
 *
 * Transaction state, selected-parameter validity, data pointers, and transfer counters are cleared.
 *
 * @param[out] transfer Active NXP SDK slave transfer descriptor.
 */
static void motor_bus_transfer_reset(i2c_slave_transfer_t *transfer) {
    motor_bus_state = kMotorBusIdle;
    motor_bus_extended_header_pending = false;
    motor_bus_receive_reset();
    motor_bus_selected_parameter_valid = false;
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
 * @param[in] user_data Unused callback context.
 */
static void motor_bus_transfer_callback(I2C_Type *base, i2c_slave_transfer_t *transfer,
                                        void *user_data) {
    (void)base;
    (void)user_data;

    switch (transfer->event) {
    case kI2C_SlaveStartEvent:
        if (motor_bus_state == kMotorBusReceiving && transfer->transferredCount == 1U) {
            motor_bus_selected_parameter = motor_bus_receive[0];
            motor_bus_selected_parameter_valid = true;
        } else {
            motor_bus_selected_parameter_valid = false;
        }
        motor_bus_state = kMotorBusIdle;
        motor_bus_receive_reset();
        transfer->data = NULL;
        transfer->dataSize = 0U;
        transfer->transferredCount = 0U;
        motor_bus_extended_header_pending = true;
        motor_bus_active = true;
        motor_bus_active_ticks = 0U;
        break;
    case kI2C_SlaveAddressMatchEvent:
        motor_bus_extended_header_pending = false;
        if ((I2C_SlaveGetStatusFlags(base) & (uint32_t)kI2C_TransferDirectionFlag) != 0U) {
            transfer->transferredCount = 0U;
            motor_bus_transmit_prepare(transfer);
        } else {
            motor_bus_selected_parameter_valid = false;
        }
        motor_bus_active = true;
        break;
    case kI2C_SlaveGenaralcallEvent:
        motor_bus_extended_header_pending = false;
        motor_bus_selected_parameter_valid = false;
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
        } else {
            motor_bus_transmit_prepare(transfer);
        }
        break;
    case kI2C_SlaveCompletionEvent: {
        if (motor_bus_state == kMotorBusReceiving &&
            transfer->completionStatus == kStatus_Success) {
            motor_bus_receive_complete(transfer);
        }
        const bool completed = transfer->completionStatus == kStatus_Success;
        motor_bus_transfer_reset(transfer);
        motor_bus_active = !completed;
        if (completed) {
            motor_bus_active_ticks = 0U;
        } else {
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

    I2C_SlaveTransferCreateHandle(I2C0, &motor_bus_handle, motor_bus_transfer_callback, NULL);
    (void)I2C_SlaveTransferNonBlocking(I2C0, &motor_bus_handle, kI2C_SlaveAllEvents);
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
    motor_bus_selected_parameter_valid = false;
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
    motor_bus_selected_parameter_valid = false;
    motor_bus_receive_reset();
    motor_bus_hardware_initialize();
}

/**
 * @brief Dispatches the official I2C0 vector through the NXP SDK slave driver.
 *
 * The SDK advances the active transaction and invokes the motor parameter callback as needed.
 */
void I2C0_IRQHandler(void) { I2C_SlaveTransferHandleIRQ(I2C0, &motor_bus_handle); }
