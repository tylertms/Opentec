#include "platform/aux_bus.h"

#include <libpic30.h>
#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

#include "platform/time.h"

/**
 * @brief States in the interrupt-driven auxiliary-bus transaction.
 */
typedef enum {
    AUX_BUS_START,         /**< Issue the initial start condition. */
    AUX_BUS_WRITE_ADDRESS, /**< Send the device write address. */
    AUX_BUS_REGISTER_HIGH, /**< Send the high register-address byte. */
    AUX_BUS_REGISTER_LOW,  /**< Send the low register-address byte. */
    AUX_BUS_WRITE_DATA,    /**< Send write payload bytes. */
    AUX_BUS_RESTART,       /**< Issue the repeated start condition. */
    AUX_BUS_READ_ADDRESS,  /**< Send the device read address. */
    AUX_BUS_RECEIVE,       /**< Receive one payload byte. */
    AUX_BUS_ACKNOWLEDGE,   /**< Acknowledge or negatively acknowledge the received byte. */
    AUX_BUS_STOP,          /**< Complete the stop condition and publish the result. */
} AuxBusPhase;

/**
 * @brief Auxiliary-bus controller configuration values.
 */
enum {
    AUX_BUS_BAUD_RATE = 0xa3,                 /**< I2C2 baud generator value. */
    AUX_BUS_INTERRUPT_PRIORITY = 7,           /**< I2C2 interrupt priority. */
    AUX_BUS_TIMEOUT_BASE_MS = 20,             /**< Base transaction timeout in milliseconds. */
    AUX_BUS_RECOVERY_PULSES = 9,              /**< Maximum SCL recovery pulses. */
    AUX_BUS_RECOVERY_DELAY_CYCLES = 0x23 * 2, /**< Delay cycles for each bus-recovery interval. */
};

/**
 * @brief Current auxiliary-bus transaction status.
 */
static volatile PlatformAuxBusStatus status;

/**
 * @brief Current auxiliary-bus transaction phase.
 */
static volatile AuxBusPhase phase;

/**
 * @brief Status to publish after the stop condition completes.
 */
static volatile PlatformAuxBusStatus stop_status;

/**
 * @brief Seven-bit address of the active auxiliary-bus device.
 */
static uint8_t device_address;

/**
 * @brief Register address of the active auxiliary-bus transaction.
 */
static uint16_t memory_address;

/**
 * @brief Payload length of the active auxiliary-bus transaction.
 */
static uint16_t data_length;

/**
 * @brief Number of payload bytes already transferred.
 */
static volatile uint16_t data_index;

/**
 * @brief Source buffer for the active auxiliary-bus write.
 */
static const uint8_t *write_data;

/**
 * @brief Destination buffer for the active auxiliary-bus read.
 */
static uint8_t *read_data;

/**
 * @brief True when the active transaction is a read.
 */
static bool reading;

/**
 * @brief True when the active register address requires two bytes.
 */
static bool wide_address;

/**
 * @brief Monotonic deadline for the active transaction.
 */
static uint32_t deadline;

/**
 * @brief Starts one interrupt-driven auxiliary-bus transaction.
 *
 * Captures the 7-bit device address, register address, and transfer length, then issues a start
 * condition. A zero transfer length is valid for a register-only write.
 *
 * @param[in] address Seven-bit device address.
 * @param[in] register_address Eight- or sixteen-bit register address.
 * @param[in] length Number of payload bytes to write or read.
 * @return True when the controller accepted the transaction; otherwise false.
 */
static bool start_transaction(uint8_t address, uint16_t register_address, uint16_t length) {
    if (status == PLATFORM_AUX_BUS_BUSY || address >= 0x80) {
        return false;
    }

    device_address = address;
    memory_address = register_address;
    data_length = length;
    data_index = 0;
    wide_address = register_address > UINT8_MAX;
    stop_status = PLATFORM_AUX_BUS_SUCCEEDED;
    phase = AUX_BUS_START;
    status = PLATFORM_AUX_BUS_BUSY;
    deadline = platform_time_ms() + AUX_BUS_TIMEOUT_BASE_MS + length / 16;
    I2C2CONbits.SEN = 1;
    return true;
}

/**
 * @brief Checks a payload-bearing auxiliary-bus transaction request.
 *
 * Rejects a busy controller, an address outside the seven-bit range, a null payload, or an empty
 * payload.
 *
 * @param[in] address Seven-bit device address.
 * @param[in] data Payload source or destination.
 * @param[in] length Number of payload bytes.
 * @return True when the transaction arguments can be queued; otherwise false.
 */
static bool transaction_valid(uint8_t address, const void *data, uint16_t length) {
    return status != PLATFORM_AUX_BUS_BUSY && address < 0x80 && data != 0 && length != 0;
}

/**
 * @brief Finishes an auxiliary-bus transaction with a stop condition.
 *
 * Retains the requested final status until the controller reports that the stop condition has
 * completed.
 *
 * @param[in] result Status to publish after the stop condition.
 */
static void stop_transaction(PlatformAuxBusStatus result) {
    stop_status = result;
    phase = AUX_BUS_STOP;
    I2C2CONbits.PEN = 1;
}

/**
 * @brief Checks the most recent auxiliary-bus transmission.
 *
 * Reads the slave acknowledgement flag after an address, register, or payload byte.
 *
 * @return True when the receiver did not acknowledge the byte.
 */
static bool transmission_failed(void) { return I2C2STATbits.ACKSTAT != 0; }

/**
 * @brief Sends the low register-address byte.
 *
 * Writes the low eight address bits and advances the interrupt state machine.
 */
static void send_register_low(void) {
    I2C2TRN = (uint8_t)memory_address;
    phase = AUX_BUS_REGISTER_LOW;
}

/**
 * @brief Continues after the register address is acknowledged.
 *
 * Restarts the bus for a read, transmits the first write byte, or finishes a register-only write.
 */
static void send_data_or_stop(void) {
    if (reading) {
        I2C2CONbits.RSEN = 1;
        phase = AUX_BUS_RESTART;
        return;
    }

    if (data_length == 0) {
        stop_transaction(PLATFORM_AUX_BUS_SUCCEEDED);
        return;
    }

    I2C2TRN = write_data[data_index++];
    phase = AUX_BUS_WRITE_DATA;
}

/**
 * @brief Releases a stalled auxiliary bus.
 *
 * Samples SDA on RF4 and, while it remains low, drives and releases SCL on RF5 up to nine times.
 * Each low interval uses one configured recovery delay and each high interval uses two.
 */
static void recover_bus(void) {
    TRISFbits.TRISF4 = 1;
    for (uint8_t pulse = 0; pulse < AUX_BUS_RECOVERY_PULSES && PORTFbits.RF4 == 0; pulse++) {
        LATFbits.LATF5 = 0;
        TRISFbits.TRISF5 = 0;
        __delay32(AUX_BUS_RECOVERY_DELAY_CYCLES);
        TRISFbits.TRISF5 = 1;
        __delay32(AUX_BUS_RECOVERY_DELAY_CYCLES);
        __delay32(AUX_BUS_RECOVERY_DELAY_CYCLES);
    }
}

/**
 * @brief Resets the auxiliary-bus controller.
 *
 * Disables I2C2, releases a stalled bus, clears collision and overflow flags, selects SMBus input
 * thresholds, programs baud value 0xa3 and interrupt priority seven, and re-enables master events.
 */
static void reset_controller(void) {
    I2C2CONbits.I2CEN = 0;
    recover_bus();
    I2C2CON = 0;
    I2C2STATbits.IWCOL = 0;
    I2C2STATbits.BCL = 0;
    I2C2STATbits.I2COV = 0;
    I2C2CONbits.SMEN = 1;
    I2C2BRG = AUX_BUS_BAUD_RATE;
    IPC12bits.MI2C2IP = AUX_BUS_INTERRUPT_PRIORITY;
    IFS3bits.MI2C2IF = 0;
    IEC3bits.MI2C2IE = 1;
    I2C2CONbits.I2CEN = 1;
}

/**
 * @brief Initializes the auxiliary-bus service.
 *
 * Marks the shared motor and secure-element bus idle and configures its I2C2 controller.
 */
void platform_aux_bus_init(void) {
    status = PLATFORM_AUX_BUS_IDLE;
    reset_controller();
}

/**
 * @brief Services auxiliary-bus transfer timeouts.
 *
 * Resets I2C2 and reports failure when a busy transfer reaches its length-adjusted deadline.
 */
void platform_aux_bus_service(void) {
    if (status != PLATFORM_AUX_BUS_BUSY || !platform_time_reached(platform_time_ms(), deadline)) {
        return;
    }

    IEC3bits.MI2C2IE = 0;
    reset_controller();
    status = PLATFORM_AUX_BUS_FAILED;
}

/**
 * @brief Starts a register-addressed auxiliary-bus write.
 *
 * A null data pointer with zero length sends only the register address. Nonempty writes require a
 * payload source.
 *
 * @param[in] address Seven-bit device address.
 * @param[in] register_address Eight- or sixteen-bit register address.
 * @param[in] data Payload source, or null for a register-only write.
 * @param[in] length Number of payload bytes.
 * @return True when the transaction starts; otherwise false.
 */
bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    if (length == 0) {
        if (data != 0 || status == PLATFORM_AUX_BUS_BUSY || address >= 0x80) {
            return false;
        }
    } else if (!transaction_valid(address, data, length)) {
        return false;
    }

    reading = false;
    write_data = data;
    read_data = 0;
    return start_transaction(address, register_address, length);
}

/**
 * @brief Starts a register-addressed auxiliary-bus read.
 *
 * Validates a nonempty destination, retains the read request, and starts the write-address phase
 * that precedes the repeated start.
 *
 * @param[in] address Seven-bit device address.
 * @param[in] register_address Eight- or sixteen-bit register address.
 * @param[out] data Destination for received bytes.
 * @param[in] length Number of bytes to receive.
 * @return True when the transaction starts; otherwise false.
 */
bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    if (!transaction_valid(address, data, length)) {
        return false;
    }

    reading = true;
    write_data = 0;
    read_data = data;
    return start_transaction(address, register_address, length);
}

/**
 * @brief Reads the auxiliary-bus transaction status.
 *
 * Reports whether the controller is idle, busy, succeeded, or failed.
 *
 * @return Current transaction status.
 */
PlatformAuxBusStatus platform_aux_bus_status(void) { return status; }

/**
 * @brief Clears a completed auxiliary-bus transaction status.
 *
 * Returns success or failure to idle without disturbing an active transaction.
 */
void platform_aux_bus_clear(void) {
    if (status != PLATFORM_AUX_BUS_BUSY) {
        status = PLATFORM_AUX_BUS_IDLE;
    }
}

/**
 * @brief Advances the auxiliary-bus transaction state machine.
 *
 * Clears controller faults, handles address and payload acknowledgements, emits 8- or 16-bit
 * register addresses, sequences repeated starts and reads, sends the final NACK, completes the stop
 * condition, and clears the master interrupt request.
 */
void __attribute__((interrupt, no_auto_psv)) _MI2C2Interrupt(void) {
    bool bus_error = I2C2STATbits.IWCOL != 0 || I2C2STATbits.BCL != 0 || I2C2STATbits.I2COV != 0;
    I2C2STATbits.IWCOL = 0;
    I2C2STATbits.BCL = 0;
    I2C2STATbits.I2COV = 0;

    if (bus_error && phase != AUX_BUS_STOP) {
        stop_transaction(PLATFORM_AUX_BUS_FAILED);
        IFS3bits.MI2C2IF = 0;
        return;
    }

    switch (phase) {
    case AUX_BUS_START:
        I2C2TRN = (uint8_t)(device_address << 1);
        phase = AUX_BUS_WRITE_ADDRESS;
        break;
    case AUX_BUS_WRITE_ADDRESS:
        if (transmission_failed()) {
            stop_transaction(PLATFORM_AUX_BUS_FAILED);
        } else if (wide_address) {
            I2C2TRN = (uint8_t)(memory_address >> 8);
            phase = AUX_BUS_REGISTER_HIGH;
        } else {
            send_register_low();
        }
        break;
    case AUX_BUS_REGISTER_HIGH:
        if (transmission_failed()) {
            stop_transaction(PLATFORM_AUX_BUS_FAILED);
        } else {
            send_register_low();
        }
        break;
    case AUX_BUS_REGISTER_LOW:
        if (transmission_failed()) {
            stop_transaction(PLATFORM_AUX_BUS_FAILED);
        } else {
            send_data_or_stop();
        }
        break;
    case AUX_BUS_WRITE_DATA:
        if (transmission_failed()) {
            stop_transaction(PLATFORM_AUX_BUS_FAILED);
        } else if (data_index < data_length) {
            I2C2TRN = write_data[data_index++];
        } else {
            stop_transaction(PLATFORM_AUX_BUS_SUCCEEDED);
        }
        break;
    case AUX_BUS_RESTART:
        I2C2TRN = (uint8_t)((device_address << 1) | 1u);
        phase = AUX_BUS_READ_ADDRESS;
        break;
    case AUX_BUS_READ_ADDRESS:
        if (transmission_failed()) {
            stop_transaction(PLATFORM_AUX_BUS_FAILED);
        } else {
            I2C2CONbits.RCEN = 1;
            phase = AUX_BUS_RECEIVE;
        }
        break;
    case AUX_BUS_RECEIVE:
        read_data[data_index++] = I2C2RCV;
        I2C2CONbits.ACKDT = data_index == data_length;
        I2C2CONbits.ACKEN = 1;
        phase = AUX_BUS_ACKNOWLEDGE;
        break;
    case AUX_BUS_ACKNOWLEDGE:
        if (data_index == data_length) {
            stop_transaction(PLATFORM_AUX_BUS_SUCCEEDED);
        } else {
            I2C2CONbits.RCEN = 1;
            phase = AUX_BUS_RECEIVE;
        }
        break;
    case AUX_BUS_STOP:
        status = stop_status;
        break;
    }

    IFS3bits.MI2C2IF = 0;
}
