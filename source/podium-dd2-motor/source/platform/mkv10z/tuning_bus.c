#include "platform/tuning_bus.h"

#include <fsl_i2c.h>
#include <string.h>

typedef enum {
    kMotorBusIdle,
    kMotorBusReceiving,
    kMotorBusTransmitting,
} MotorBusState;

static i2c_slave_handle_t motor_bus_handle;
static MotorParameterBank *motor_bus_parameters;
static MotorParameterChangedHandler motor_bus_changed_handler;
static void *motor_bus_context;
static uint8_t motor_bus_receive[MOTOR_PARAMETER_REQUEST_SIZE];
static uint8_t motor_bus_transmit[MOTOR_PARAMETER_RESPONSE_SIZE];
static MotorBusState motor_bus_state;
static volatile bool motor_bus_active;
static bool motor_bus_extended_header_pending;
static uint16_t motor_bus_active_ticks;
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
 * A valid selection publishes its encoded five-byte value, while an invalid index suppresses data.
 *
 * @param transfer Active NXP SDK slave transfer descriptor.
 */
static void motor_bus_transmit_prepare(i2c_slave_transfer_t *transfer) {
    MotorParameterResponse response = {0};
    if (!motor_parameter_read(motor_bus_parameters, motor_bus_receive[0], &response)) {
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
 * @param transfer Completed NXP SDK slave transfer descriptor.
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
 * @brief Handles NXP SDK slave-transfer events for the motor parameter bank.
 *
 * Receive transactions accept an index plus up to four value bytes. Transmit transactions return
 * the selected value and its declared width.
 *
 * @param base Active I2C peripheral instance.
 * @param transfer NXP SDK transfer descriptor for the current event.
 * @param user_data Unused callback context.
 */
static void motor_bus_transfer_callback(I2C_Type *base, i2c_slave_transfer_t *transfer,
                                        void *user_data) {
    (void)base;
    (void)user_data;

    switch (transfer->event) {
    case kI2C_SlaveStartEvent:
        motor_bus_extended_header_pending = true;
        break;
    case kI2C_SlaveAddressMatchEvent:
        motor_bus_extended_header_pending = false;
        if ((I2C_SlaveGetStatusFlags(base) & (uint32_t)kI2C_TransferDirectionFlag) != 0U) {
            motor_bus_receive_size = (uint8_t)transfer->transferredCount;
            transfer->transferredCount = 0U;
            motor_bus_transmit_prepare(transfer);
        }
        motor_bus_active = true;
        break;
    case kI2C_SlaveGenaralcallEvent:
        motor_bus_extended_header_pending = false;
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
        motor_bus_transmit_prepare(transfer);
        break;
    case kI2C_SlaveCompletionEvent:
        if (motor_bus_state == kMotorBusReceiving &&
            transfer->completionStatus == kStatus_Success) {
            motor_bus_receive_complete(transfer);
        }
        motor_bus_state = kMotorBusIdle;
        motor_bus_active = false;
        motor_bus_extended_header_pending = false;
        motor_bus_receive_reset();
        transfer->data = NULL;
        transfer->dataSize = 0U;
        break;
    default:
        break;
    }
}

/**
 * @brief Configures and starts the NXP SDK I2C slave transaction engine.
 *
 * The peripheral uses extended address 0x78 and all transfer events.
 */
static void motor_bus_hardware_initialize(void) {
    i2c_slave_config_t config;

    I2C_SlaveGetDefaultConfig(&config);
    config.slaveAddress = 0x78U;
    I2C_SlaveInit(I2C0, &config, CLOCK_GetFreq(kCLOCK_BusClk));

    I2C0->C2 |= I2C_C2_ADEXT_MASK;
    I2C0->F = 0x27U;

    I2C_SlaveTransferCreateHandle(I2C0, &motor_bus_handle, motor_bus_transfer_callback, NULL);
    (void)I2C_SlaveTransferNonBlocking(I2C0, &motor_bus_handle, kI2C_SlaveAllEvents);
    I2C0->FLT = (I2C0->FLT & 0xa0U) | 4U;
}

/**
 * @brief Starts the motor parameter service on I2C address 0x78.
 *
 * The service exposes the official sixty-four-entry parameter bank through five-byte read and write
 * transactions.
 *
 * @param parameters Parameter bank shared with the motor runtime.
 * @param changed_handler Function invoked after live control settings change.
 * @param context Caller context passed to the change handler.
 */
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

/**
 * @brief Recovers an I2C parameter transaction that remains active for ten service ticks.
 *
 * The stalled SDK transfer is aborted and the official slave configuration is restored.
 */
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
