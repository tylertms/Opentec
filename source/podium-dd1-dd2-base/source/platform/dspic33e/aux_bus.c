#include "platform/aux_bus.h"

#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

#include "platform/time.h"

typedef enum {
    AUX_BUS_START,
    AUX_BUS_WRITE_ADDRESS,
    AUX_BUS_REGISTER_HIGH,
    AUX_BUS_REGISTER_LOW,
    AUX_BUS_WRITE_DATA,
    AUX_BUS_RESTART,
    AUX_BUS_READ_ADDRESS,
    AUX_BUS_RECEIVE,
    AUX_BUS_ACKNOWLEDGE,
    AUX_BUS_STOP,
} AuxBusPhase;

enum {
    AUX_BUS_BAUD_RATE = 0xa3,
    AUX_BUS_INTERRUPT_PRIORITY = 7,
    AUX_BUS_TIMEOUT_BASE_MS = 20,
};

static volatile PlatformAuxBusStatus status;
static volatile AuxBusPhase phase;
static volatile PlatformAuxBusStatus stop_status;
static uint8_t device_address;
static uint16_t memory_address;
static uint16_t data_length;
static volatile uint16_t data_index;
static const uint8_t *write_data;
static uint8_t *read_data;
static bool reading;
static bool wide_address;
static uint32_t deadline;

static bool start_transaction(uint8_t address, uint16_t register_address, uint16_t length) {
    if (status == PLATFORM_AUX_BUS_BUSY || address >= 0x80 || length == 0) {
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

static bool transaction_valid(uint8_t address, const void *data, uint16_t length) {
    return status != PLATFORM_AUX_BUS_BUSY && address < 0x80 && data != 0 && length != 0;
}

static void stop_transaction(PlatformAuxBusStatus result) {
    stop_status = result;
    phase = AUX_BUS_STOP;
    I2C2CONbits.PEN = 1;
}

static bool transmission_failed(void) { return I2C2STATbits.ACKSTAT != 0; }

static void send_register_low(void) {
    I2C2TRN = (uint8_t)memory_address;
    phase = AUX_BUS_REGISTER_LOW;
}

static void send_data_or_stop(void) {
    if (reading) {
        I2C2CONbits.RSEN = 1;
        phase = AUX_BUS_RESTART;
        return;
    }

    I2C2TRN = write_data[data_index++];
    phase = AUX_BUS_WRITE_DATA;
}

static void reset_controller(void) {
    I2C2CONbits.I2CEN = 0;
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

void platform_aux_bus_init(void) {
    status = PLATFORM_AUX_BUS_IDLE;
    reset_controller();
}

void platform_aux_bus_service(void) {
    if (status != PLATFORM_AUX_BUS_BUSY || !platform_time_reached(platform_time_ms(), deadline)) {
        return;
    }

    IEC3bits.MI2C2IE = 0;
    reset_controller();
    status = PLATFORM_AUX_BUS_FAILED;
}

bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    if (!transaction_valid(address, data, length)) {
        return false;
    }

    reading = false;
    write_data = data;
    read_data = 0;
    return start_transaction(address, register_address, length);
}

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

PlatformAuxBusStatus platform_aux_bus_status(void) { return status; }

void platform_aux_bus_clear(void) {
    if (status != PLATFORM_AUX_BUS_BUSY) {
        status = PLATFORM_AUX_BUS_IDLE;
    }
}

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
