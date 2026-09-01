#include "machine.h"

#include <dspic33.h>
#include <kinetis.h>
#include <process.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "firmware.h"
#include "wheel.h"

enum {
    LINK_QUEUE_CAPACITY = 4096,
    BASE_CLOCK_HZ = 60000000,
    WQR_CLOCK_HZ = 96000000,
    MOTOR_CLOCK_HZ = 71991296,
    BASE_UART_CHANNEL = 2,
    BASE_MOTOR_SPI_CHANNEL = 0,
    MOTOR_FRAME_SIZE = 13,
    MOTOR_LINK_VALUE_MASK = 0x00ff,
    MOTOR_LINK_FRAME_START = 0x0100,
    MOTOR_LINK_FRAME_END = 0x0200,
    SYNC_QUANTUM_CYCLES = 2048,
    COMMUNICATION_QUANTUM_CYCLES = 96,
    ENCODER_TRANSITION_CYCLES = 8,
    COMMUNICATION_GUARD_QUANTA = 4,
    BASE_NVM_CONTROL = 0x0728,
    BASE_INTERRUPT_CONTROL_2 = 0x08c2,
    BASE_MOTOR_SPI_STATUS = 0x0240,
    BASE_NVM_WRITE = 0x8000,
    BASE_PERIPHERAL_ENABLE = 0x8000,
    BASE_GLOBAL_INTERRUPT_ENABLE = 0x8000,
    BASE_NVM_WRITE_OPCODE = 0xa8e729,
    MOTOR_FTM2_MODULUS = 0x4003a008,
    WQR_SPI0_CTAR0 = 0x4002c00c,
    DISPLAY_TRANSFER_DATA_MODE = 1,
};

typedef struct {
    uint16_t values[LINK_QUEUE_CAPACITY];
    size_t head;
    size_t count;
} LinkQueue;

typedef enum {
    WORKER_BASE,
    WORKER_WQR,
    WORKER_MOTOR,
    WORKER_COUNT,
} WorkerKind;

typedef struct {
    struct Dd1SimMachine *machine;
    WorkerKind kind;
    HANDLE start;
    HANDLE done;
    HANDLE thread;
    uint32_t cycles;
    bool stop;
    bool result;
} McuWorker;

struct Dd1SimMachine {
    Dspic33 *base;
    Kinetis *wqr;
    Kinetis *motor;
    Dd1SimWheel wheel;
    LinkQueue base_to_wqr;
    LinkQueue wqr_to_base;
    LinkQueue base_to_motor;
    LinkQueue motor_to_base;
    uint8_t display[DD1_SIM_DISPLAY_SIZE];
    uint8_t display_capture[DD1_SIM_DISPLAY_SIZE];
    uint8_t wheel_response[DD1_SIM_WHEEL_PACKET_SIZE];
    uint8_t wheel_output[DD1_SIM_WHEEL_OUTPUT_SIZE];
    uint8_t motor_frame[MOTOR_FRAME_SIZE];
    size_t display_index;
    size_t wheel_response_index;
    size_t wheel_output_index;
    size_t motor_frame_index;
    size_t motor_transaction_index;
    uint64_t base_instructions;
    uint64_t wqr_instructions;
    uint64_t motor_instructions;
    uint64_t display_bytes;
    uint64_t base_uart_bytes;
    uint64_t wqr_uart_bytes;
    uint64_t base_motor_words;
    uint64_t motor_base_words;
    uint64_t base_raw_instructions;
    uint64_t wqr_raw_instructions;
    uint64_t motor_raw_instructions;
    uint64_t wqr_clock_remainder;
    uint64_t motor_clock_remainder;
    uint32_t buttons;
    int32_t motor_position;
    int16_t motor_current;
    int16_t angle_tenths;
    int32_t encoder_position;
    int32_t encoder_target;
    uint8_t encoder_phase;
    uint8_t motor_index_stage;
    uint16_t base_saved_interrupt_control;
    bool base_nvm_workaround;
    bool display_ready;
    bool display_reset_active;
    bool wheel_alternate_initialized;
    bool base_motor_selected;
    uint8_t communication_guard;
    bool parallel;
    bool running;
    McuWorker workers[WORKER_COUNT];
    char status[DD1_SIM_STATUS_SIZE];
};

static void capture_display(Dd1SimMachine *machine);
static void base_trace(void *context, uint32_t program_counter, uint32_t opcode);
static uint8_t display_transfer_hook(Dspic33 *cpu, const Dspic33PmpTransfer *transfer,
                                     void *context);

static void write_error(char *error, size_t error_size, const char *format, ...) {
    if (error == NULL || error_size == 0) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
}

static bool queue_push(LinkQueue *queue, uint16_t value) {
    if (queue->count == LINK_QUEUE_CAPACITY) {
        return false;
    }
    size_t index = (queue->head + queue->count) % LINK_QUEUE_CAPACITY;
    queue->values[index] = value;
    ++queue->count;
    return true;
}

static uint16_t queue_front(const LinkQueue *queue) { return queue->values[queue->head]; }

static void queue_pop(LinkQueue *queue) {
    queue->head = (queue->head + 1) % LINK_QUEUE_CAPACITY;
    --queue->count;
}

static bool require_queue_space(Dd1SimMachine *machine, const LinkQueue *queue, const char *name) {
    if (queue->count < LINK_QUEUE_CAPACITY) {
        return true;
    }
    machine->running = false;
    snprintf(machine->status, sizeof(machine->status), "%s link queue is full", name);
    return false;
}

static bool wqr_spi_transfer_is_wide(const Kinetis *device, const KinetisSpiTransfer *transfer,
                                     bool *wide) {
    uint32_t ctar = 0;
    uint32_t address =
        WQR_SPI0_CTAR0 + (transfer->clock_and_transfer_attributes == 0 ? 0 : sizeof(uint32_t));
    if (!kinetis_read(device, address, &ctar, sizeof(ctar))) {
        return false;
    }
    *wide = (((ctar >> 27U) & 0x0fU) + 1U) > 8U;
    return true;
}

static bool load_base(Dd1SimMachine *machine, const char *path, char *error, size_t error_size) {
    machine->base = dspic33_create_for_device(DSPIC33EP_MU_DEVICE_512MU810);
    if (machine->base == NULL) {
        write_error(error, error_size, "could not create the base MCU");
        return false;
    }
    if (!dd1_firmware_load_base(machine->base, path, error, error_size)) {
        return false;
    }
    dspic33_reset(machine->base, 0);
    dspic33_gpio_drive(machine->base, 1, UINT16_C(0x0002), UINT16_C(0x0006));
    dspic33_gpio_drive(machine->base, 3, UINT16_C(0x0002), UINT16_C(0x3202));
    dspic33_gpio_drive(machine->base, 4, 0, UINT16_C(0x0300));
    dspic33_gpio_drive(machine->base, 6, UINT16_C(0x0200), UINT16_C(0x0200));
    return true;
}

static bool load_wqr(Dd1SimMachine *machine, const char *path, char *error, size_t error_size) {
    KinetisConfiguration configuration = kinetis_configuration(KINETIS_PROFILE_MKV30F12810);
    configuration.package = KINETIS_PACKAGE_LH_64_LQFP;
    configuration.vector_table_address = 0xa000U;
    machine->wqr = kinetis_create(configuration);
    if (machine->wqr == NULL ||
        !dd1_firmware_load_kinetis(machine->wqr, path, true, error, error_size) ||
        !kinetis_reset(machine->wqr)) {
        if (error != NULL && error[0] == 0) {
            write_error(error, error_size, "could not load WQR firmware");
        }
        return false;
    }
    kinetis_gpio_drive(machine->wqr, 0, 4, true);
    kinetis_gpio_drive(machine->wqr, 0, 18, true);
    kinetis_gpio_drive(machine->wqr, 0, 19, true);
    kinetis_gpio_drive(machine->wqr, 2, 2, false);
    kinetis_set_adc_input(machine->wqr, 0, KINETIS_ADC_MUX_A, 23, 2048);
    return true;
}

static bool load_motor(Dd1SimMachine *machine, const char *path, char *error, size_t error_size) {
    KinetisConfiguration configuration = kinetis_configuration(KINETIS_PROFILE_MKV10Z1287);
    configuration.package = KINETIS_PACKAGE_LH_64_LQFP;
    configuration.vector_table_address = 0xa000U;
    machine->motor = kinetis_create(configuration);
    if (machine->motor == NULL ||
        !dd1_firmware_load_kinetis(machine->motor, path, true, error, error_size) ||
        !kinetis_reset(machine->motor)) {
        if (error != NULL && error[0] == 0) {
            write_error(error, error_size, "could not load motor firmware");
        }
        return false;
    }
    const uint8_t adc0_channels[] = {4, 7, 9};
    const uint8_t adc1_channels[] = {0, 2, 10};
    for (size_t index = 0; index < sizeof(adc0_channels); ++index) {
        kinetis_set_adc_input(machine->motor, 0, KINETIS_ADC_MUX_A, adc0_channels[index], 2048);
    }
    for (size_t index = 0; index < sizeof(adc1_channels); ++index) {
        kinetis_set_adc_input(machine->motor, 1, KINETIS_ADC_MUX_A, adc1_channels[index], 2048);
    }
    kinetis_gpio_drive(machine->motor, 2, 3, true);
    kinetis_gpio_drive(machine->motor, 4, 17, false);
    kinetis_gpio_drive(machine->motor, 4, 20, true);
    kinetis_gpio_drive(machine->motor, 2, 4, true);
    kinetis_gpio_drive(machine->motor, 4, 29, true);
    kinetis_gpio_drive(machine->motor, 4, 24, true);
    kinetis_set_ftm_input(machine->motor, 2, 0, false);
    kinetis_set_ftm_input(machine->motor, 2, 1, false);
    kinetis_set_ftm_quadrature_input(machine->motor, 2, 0, false);
    kinetis_set_ftm_quadrature_input(machine->motor, 2, 1, false);
    return true;
}

static void stop_workers(Dd1SimMachine *machine) {
    for (size_t index = 0; index < WORKER_COUNT; ++index) {
        McuWorker *worker = &machine->workers[index];
        if (worker->thread != NULL) {
            worker->stop = true;
            SetEvent(worker->start);
        }
    }
    for (size_t index = 0; index < WORKER_COUNT; ++index) {
        McuWorker *worker = &machine->workers[index];
        if (worker->thread != NULL) {
            WaitForSingleObject(worker->thread, INFINITE);
            CloseHandle(worker->thread);
        }
        if (worker->start != NULL) {
            CloseHandle(worker->start);
        }
        if (worker->done != NULL) {
            CloseHandle(worker->done);
        }
    }
}

Dd1SimMachine *dd1_sim_machine_create(const char *base_path, const char *wqr_path,
                                      const char *motor_path, char *error, size_t error_size) {
    if (base_path == NULL || wqr_path == NULL || motor_path == NULL) {
        write_error(error, error_size, "three encrypted firmware paths are required");
        return NULL;
    }
    Dd1SimMachine *machine = calloc(1, sizeof(*machine));
    if (machine == NULL) {
        write_error(error, error_size, "could not allocate the simulator");
        return NULL;
    }
    dd1_sim_wheel_init(&machine->wheel);
    dd1_sim_wheel_response(&machine->wheel, machine->wheel_response);
    machine->running = load_base(machine, base_path, error, error_size) &&
                       load_wqr(machine, wqr_path, error, error_size) &&
                       load_motor(machine, motor_path, error, error_size);
    if (!machine->running) {
        dd1_sim_machine_destroy(machine);
        return NULL;
    }
    dspic33_set_trace(machine->base, base_trace, machine);
    dspic33_pmp_set_transfer_hook(machine->base, display_transfer_hook, NULL);
    machine->motor_index_stage = 1;
    machine->parallel = true;
    snprintf(machine->status, sizeof(machine->status), "Booting DD1 firmware");
    return machine;
}

void dd1_sim_machine_destroy(Dd1SimMachine *machine) {
    if (machine == NULL) {
        return;
    }
    stop_workers(machine);
    if (machine->base != NULL) {
        dspic33_destroy(machine->base);
    }
    if (machine->wqr != NULL) {
        kinetis_destroy(machine->wqr);
    }
    if (machine->motor != NULL) {
        kinetis_destroy(machine->motor);
    }
    free(machine);
}

static void base_trace(void *context, uint32_t program_counter, uint32_t opcode) {
    Dd1SimMachine *machine = context;
    (void)program_counter;
    if (machine->base_nvm_workaround &&
        (dspic33_read_word(machine->base, BASE_NVM_CONTROL) & BASE_NVM_WRITE) == 0) {
        dspic33_write_word(machine->base, BASE_INTERRUPT_CONTROL_2,
                           machine->base_saved_interrupt_control);
        machine->base_nvm_workaround = false;
    }
    if (!machine->base_nvm_workaround && opcode == BASE_NVM_WRITE_OPCODE) {
        machine->base_saved_interrupt_control =
            dspic33_read_word(machine->base, BASE_INTERRUPT_CONTROL_2);
        dspic33_write_word(machine->base, BASE_INTERRUPT_CONTROL_2,
                           machine->base_saved_interrupt_control & ~BASE_GLOBAL_INTERRUPT_ENABLE);
        machine->base_nvm_workaround = true;
    }
}

static uint64_t monotonic_delta(uint64_t previous, uint64_t current) {
    return current >= previous ? current - previous : current;
}

static uint32_t convert_base_cycles(uint32_t base_cycles, uint32_t clock_hz, uint64_t *remainder) {
    uint64_t scaled = (uint64_t)base_cycles * clock_hz + *remainder;
    uint64_t converted = scaled / BASE_CLOCK_HZ;
    *remainder = scaled % BASE_CLOCK_HZ;
    return converted > UINT32_MAX ? UINT32_MAX : (uint32_t)converted;
}

static bool run_base(Dd1SimMachine *machine, uint32_t cycles) {
    uint64_t before = dspic33_get_instruction_count(machine->base);
    uint64_t before_cycles = dspic33_get_cycle_count(machine->base);
    Dspic33Result result = dspic33_run_with_limits(
        machine->base, (Dspic33RunLimits){.instruction_limit = cycles, .cycle_limit = cycles});
    if (result.stop == DSPIC33_SLEEPING || result.stop == DSPIC33_IDLING) {
        uint64_t after_cycles = dspic33_get_cycle_count(machine->base);
        uint64_t elapsed_cycles = after_cycles >= before_cycles ? after_cycles - before_cycles : 0;
        uint32_t remaining_cycles =
            elapsed_cycles >= cycles ? 0 : cycles - (uint32_t)elapsed_cycles;
        if (!dspic33_device_advance(machine->base, remaining_cycles)) {
            result.stop = DSPIC33_EVENT_QUEUE_ERROR;
        } else {
            result.stop = dspic33_step(machine->base);
        }
    }
    uint64_t after = dspic33_get_instruction_count(machine->base);
    machine->base_instructions += monotonic_delta(before, after);
    machine->base_raw_instructions = after;
    if (result.stop == DSPIC33_RUNNING || result.stop == DSPIC33_INSTRUCTION_LIMIT ||
        result.stop == DSPIC33_SLEEPING || result.stop == DSPIC33_IDLING) {
        return true;
    }
    snprintf(machine->status, sizeof(machine->status), "Base stopped: %s at 0x%08x",
             dspic33_stop_reason_name(result.stop), dspic33_get_program_counter(machine->base));
    return false;
}

static bool run_kinetis(Dd1SimMachine *machine, Kinetis *device, uint64_t *instructions,
                        uint64_t *raw_instructions, uint32_t cycles, const char *name) {
    CortexM4 *cpu = kinetis_cpu(device);
    uint64_t before = cortex_m4_get_instruction_count(cpu);
    uint64_t before_cycles = cortex_m4_get_cycle_count(cpu);
    CortexM4Result result =
        cortex_m4_run(cpu, (CortexM4RunLimits){.instruction_limit = cycles, .cycle_limit = cycles});
    if (result.stop == CORTEX_M4_STOP_CLOCK) {
        uint64_t after_cycles = cortex_m4_get_cycle_count(cpu);
        uint64_t elapsed_cycles = after_cycles >= before_cycles ? after_cycles - before_cycles : 0;
        uint32_t remaining_cycles =
            elapsed_cycles >= cycles ? 0 : cycles - (uint32_t)elapsed_cycles;
        kinetis_advance(device, remaining_cycles);
        result = cortex_m4_step(cpu);
    }
    uint64_t after = cortex_m4_get_instruction_count(cpu);
    *instructions += monotonic_delta(before, after);
    *raw_instructions = after;
    if (result.stop == CORTEX_M4_STOP_RUNNING || result.stop == CORTEX_M4_STOP_LIMIT ||
        result.stop == CORTEX_M4_STOP_CLOCK) {
        return true;
    }
    snprintf(machine->status, sizeof(machine->status), "%s stopped: %u at 0x%08x", name,
             (unsigned)result.stop, result.pc);
    return false;
}

static void capture_display(Dd1SimMachine *machine) {
    Dspic33PmpTransfer transfer;
    bool reset_high;
    if (dspic33_gpio_pin(machine->base, 6, 14, &reset_high) && !reset_high) {
        while (dspic33_pmp_transmit(machine->base, &transfer)) {
            ++machine->display_bytes;
        }
        if (!machine->display_reset_active) {
            memset(machine->display, 0, sizeof(machine->display));
            memset(machine->display_capture, 0, sizeof(machine->display_capture));
            machine->display_index = 0;
            machine->display_ready = false;
        }
        machine->display_reset_active = true;
        return;
    }
    machine->display_reset_active = false;
    while (dspic33_pmp_transmit(machine->base, &transfer)) {
        ++machine->display_bytes;
        if ((transfer.metadata & DISPLAY_TRANSFER_DATA_MODE) == 0) {
            machine->display_index = 0;
            continue;
        }
        if (machine->display_index < DD1_SIM_DISPLAY_SIZE) {
            machine->display_capture[machine->display_index++] = (uint8_t)transfer.value;
            if (machine->display_index == DD1_SIM_DISPLAY_SIZE) {
                memcpy(machine->display, machine->display_capture, sizeof(machine->display));
                machine->display_ready = true;
            }
        }
    }
}

static uint8_t display_transfer_hook(Dspic33 *cpu, const Dspic33PmpTransfer *transfer,
                                     void *context) {
    bool data_mode;
    (void)transfer;
    (void)context;
    return dspic33_gpio_pin(cpu, 3, 3, &data_mode) && data_mode ? DISPLAY_TRANSFER_DATA_MODE : 0;
}

static void collect_base_links(Dd1SimMachine *machine) {
    Dspic33UartFrame frame;
    uint8_t value;
    while (require_queue_space(machine, &machine->base_to_wqr, "Base/WQR") &&
           dspic33_uart_transmit(machine->base, BASE_UART_CHANNEL, &frame)) {
        machine->communication_guard = COMMUNICATION_GUARD_QUANTA;
        ++machine->base_uart_bytes;
        if (!queue_push(&machine->base_to_wqr, (uint8_t)frame.value)) {
            machine->running = false;
            snprintf(machine->status, sizeof(machine->status), "Base/WQR link queue is full");
            break;
        }
    }
    while (require_queue_space(machine, &machine->base_to_motor, "Base/motor") &&
           dspic33_spi_transmit(machine->base, BASE_MOTOR_SPI_CHANNEL, &value)) {
        machine->communication_guard = COMMUNICATION_GUARD_QUANTA;
        ++machine->base_motor_words;
        if (!queue_push(&machine->base_to_motor, value)) {
            machine->running = false;
            snprintf(machine->status, sizeof(machine->status), "Base/motor link queue is full");
            break;
        }
    }
}

static void collect_wqr_links(Dd1SimMachine *machine) {
    uint8_t value;
    KinetisSpiTransfer transfer;
    while (require_queue_space(machine, &machine->wqr_to_base, "WQR/base") &&
           kinetis_uart1_transmit(machine->wqr, &value)) {
        machine->communication_guard = COMMUNICATION_GUARD_QUANTA;
        ++machine->wqr_uart_bytes;
        if (!queue_push(&machine->wqr_to_base, value)) {
            machine->running = false;
            snprintf(machine->status, sizeof(machine->status), "WQR/base link queue is full");
            break;
        }
    }
    while (kinetis_spi_transfer(machine->wqr, KINETIS_SERIAL_SPI0, &transfer)) {
        machine->communication_guard = COMMUNICATION_GUARD_QUANTA;
        bool wide = false;
        if (!wqr_spi_transfer_is_wide(machine->wqr, &transfer, &wide)) {
            machine->running = false;
            snprintf(machine->status, sizeof(machine->status), "WQR SPI format read failed");
            break;
        }
        if (wide) {
            if (!machine->wheel_alternate_initialized) {
                machine->wheel_output_index = 0;
                machine->wheel_response_index = DD1_SIM_WHEEL_PACKET_SIZE;
            }
            uint16_t response =
                machine->wheel_alternate_initialized
                    ? dd1_sim_wheel_alternate_response(&machine->wheel, (uint16_t)transfer.data)
                    : 0;
            machine->wheel_alternate_initialized = true;
            if (!kinetis_serial_receive(machine->wqr, KINETIS_SERIAL_SPI0, response, 0)) {
                machine->running = false;
                snprintf(machine->status, sizeof(machine->status), "WQR SPI response rejected");
                break;
            }
            continue;
        }
        if (machine->wheel_alternate_initialized) {
            machine->wheel_alternate_initialized = false;
            machine->wheel_output_index = 0;
            machine->wheel_response_index = 0;
        }
        if (machine->wheel_output_index >= DD1_SIM_WHEEL_OUTPUT_SIZE) {
            machine->wheel_output_index = 0;
        }
        machine->wheel_output[machine->wheel_output_index++] = (uint8_t)transfer.data;
        if (machine->wheel_output_index == DD1_SIM_WHEEL_OUTPUT_SIZE) {
            dd1_sim_wheel_accept_output(&machine->wheel, machine->wheel_output);
            dd1_sim_wheel_response(&machine->wheel, machine->wheel_response);
            machine->wheel_output_index = 0;
            machine->wheel_response_index = 0;
        }
    }
}

static uint16_t motor_crc(const uint8_t *data, size_t length) {
    uint16_t crc = 0;
    while (length-- != 0) {
        crc ^= (uint16_t)*data++ << 8U;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & UINT16_C(0x8000)) != 0 ? (uint16_t)((crc << 1U) ^ UINT16_C(0x1021))
                                                : (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

static void decode_motor_frame(Dd1SimMachine *machine) {
    uint16_t received_crc = (uint16_t)machine->motor_frame[10] | (uint16_t)machine->motor_frame[11]
                                                                     << 8U;
    if (machine->motor_frame[0] != 0x7b || machine->motor_frame[12] != 0x7d ||
        (machine->motor_frame[1] & 0x7fU) != 1 ||
        motor_crc(machine->motor_frame + 1, 9) != received_crc) {
        return;
    }
    uint32_t position =
        (uint32_t)machine->motor_frame[2] | (uint32_t)machine->motor_frame[3] << 8U |
        (uint32_t)machine->motor_frame[4] << 16U | (uint32_t)machine->motor_frame[5] << 24U;
    uint16_t magnitude =
        (uint16_t)machine->motor_frame[8] | (uint16_t)(machine->motor_frame[9] & 0x7fU) << 8U;
    machine->motor_position = (int32_t)position;
    machine->motor_current =
        (machine->motor_frame[9] & 0x80U) != 0 ? (int16_t)magnitude : (int16_t)-(int32_t)magnitude;
}

static void collect_motor_link(Dd1SimMachine *machine) {
    KinetisSpiTransfer transfer;
    while (require_queue_space(machine, &machine->motor_to_base, "Motor/base") &&
           kinetis_spi_transfer(machine->motor, KINETIS_SERIAL_SPI0, &transfer)) {
        machine->communication_guard = COMMUNICATION_GUARD_QUANTA;
        ++machine->motor_base_words;
        uint8_t byte = (uint8_t)transfer.data;
        bool transaction_start = machine->motor_transaction_index == 0;
        if (transaction_start) {
            machine->motor_frame_index = 0;
        }
        machine->motor_frame[machine->motor_frame_index++] = byte;
        ++machine->motor_transaction_index;
        bool transaction_end =
            transfer.end_of_queue || machine->motor_transaction_index == MOTOR_FRAME_SIZE;
        uint16_t queued = byte;
        if (transaction_start) {
            queued |= MOTOR_LINK_FRAME_START;
        }
        if (transaction_end) {
            queued |= MOTOR_LINK_FRAME_END;
        }
        if (!queue_push(&machine->motor_to_base, queued)) {
            machine->running = false;
            snprintf(machine->status, sizeof(machine->status), "Motor/base link queue is full");
            break;
        }
        if (transaction_end) {
            if (machine->motor_transaction_index == MOTOR_FRAME_SIZE) {
                decode_motor_frame(machine);
            }
            machine->motor_frame_index = 0;
            machine->motor_transaction_index = 0;
        }
    }
}

static void feed_links(Dd1SimMachine *machine) {
    while (machine->base_to_wqr.count != 0 &&
           kinetis_uart1_receive(machine->wqr, (uint8_t)queue_front(&machine->base_to_wqr), 0)) {
        queue_pop(&machine->base_to_wqr);
    }
    while (machine->wqr_to_base.count != 0 &&
           dspic33_uart_receive(machine->base, BASE_UART_CHANNEL,
                                (uint8_t)queue_front(&machine->wqr_to_base), 0)) {
        queue_pop(&machine->wqr_to_base);
    }
    while ((dspic33_read_word(machine->base, BASE_MOTOR_SPI_STATUS) & BASE_PERIPHERAL_ENABLE) !=
               0 &&
           machine->motor_to_base.count != 0) {
        uint16_t value = queue_front(&machine->motor_to_base);
        if (!machine->base_motor_selected) {
            if ((value & MOTOR_LINK_FRAME_START) == 0 ||
                !dspic33_spi_select(machine->base, BASE_MOTOR_SPI_CHANNEL, true, 0)) {
                machine->running = false;
                snprintf(machine->status, sizeof(machine->status), "Base motor select failed");
                break;
            }
            machine->base_motor_selected = true;
        }
        if (!dspic33_spi_receive(machine->base, BASE_MOTOR_SPI_CHANNEL,
                                 value & MOTOR_LINK_VALUE_MASK, 0)) {
            break;
        }
        if ((value & MOTOR_LINK_FRAME_END) != 0) {
            if (!dspic33_spi_select(machine->base, BASE_MOTOR_SPI_CHANNEL, false, 0)) {
                machine->running = false;
                snprintf(machine->status, sizeof(machine->status), "Base motor deselect failed");
                break;
            }
            machine->base_motor_selected = false;
        }
        queue_pop(&machine->motor_to_base);
    }
    while (machine->base_to_motor.count != 0 &&
           kinetis_serial_receive(machine->motor, KINETIS_SERIAL_SPI0,
                                  queue_front(&machine->base_to_motor), 0)) {
        queue_pop(&machine->base_to_motor);
    }
    while (machine->wheel_response_index < DD1_SIM_WHEEL_PACKET_SIZE &&
           kinetis_serial_receive(machine->wqr, KINETIS_SERIAL_SPI0,
                                  machine->wheel_response[machine->wheel_response_index], 0)) {
        ++machine->wheel_response_index;
    }
}

static void update_motor_encoder(Dd1SimMachine *machine) {
    uint32_t modulus = 0;
    if (!kinetis_read(machine->motor, MOTOR_FTM2_MODULUS, &modulus, sizeof(modulus)) ||
        modulus == 0 || modulus > UINT16_MAX) {
        return;
    }
    int32_t bounded_angle = machine->angle_tenths;
    if (bounded_angle < -4500) {
        bounded_angle = -4500;
    } else if (bounded_angle > 4500) {
        bounded_angle = 4500;
    }
    machine->encoder_target = (int32_t)((int64_t)bounded_angle * (modulus + 1U) / 3600);
}

static bool run_motor(Dd1SimMachine *machine, uint32_t cycles) {
    static const uint8_t phases[] = {0, 1, 3, 2};
    while (cycles != 0) {
        bool moving = machine->encoder_position != machine->encoder_target;
        uint32_t step_cycles =
            moving || machine->motor_index_stage != 0
                ? (cycles < ENCODER_TRANSITION_CYCLES ? cycles : ENCODER_TRANSITION_CYCLES)
                : cycles;
        if (moving) {
            bool increasing = machine->encoder_position < machine->encoder_target;
            machine->encoder_phase =
                (uint8_t)((machine->encoder_phase + (increasing ? 1U : 3U)) & 3U);
            uint8_t phase = phases[machine->encoder_phase];
            kinetis_set_ftm_quadrature_input(machine->motor, 2, 0, (phase & 1U) != 0);
            kinetis_set_ftm_quadrature_input(machine->motor, 2, 1, (phase & 2U) != 0);
            machine->encoder_position += increasing ? 1 : -1;
            uint32_t modulus = 0;
            if (kinetis_read(machine->motor, MOTOR_FTM2_MODULUS, &modulus, sizeof(modulus)) &&
                modulus != 0 && machine->encoder_position % (int32_t)(modulus + 1U) == 0) {
                machine->motor_index_stage = 1;
            }
        }
        if (machine->motor_index_stage == 1) {
            kinetis_gpio_drive(machine->motor, 4, 24, false);
            machine->motor_index_stage = 2;
        }
        bool result = run_kinetis(machine, machine->motor, &machine->motor_instructions,
                                  &machine->motor_raw_instructions, step_cycles, "Motor");
        if (machine->motor_index_stage == 2) {
            kinetis_gpio_drive(machine->motor, 4, 24, true);
            machine->motor_index_stage = 0;
        }
        if (!result) {
            return false;
        }
        cycles -= step_cycles;
    }
    return true;
}

static bool run_worker(Dd1SimMachine *machine, WorkerKind kind, uint32_t base_cycles) {
    if (kind == WORKER_BASE) {
        return run_base(machine, base_cycles);
    }
    Kinetis *device = kind == WORKER_WQR ? machine->wqr : machine->motor;
    uint32_t fallback = kind == WORKER_WQR ? WQR_CLOCK_HZ : MOTOR_CLOCK_HZ;
    uint32_t clock = kinetis_core_clock_hz(device);
    if (clock == 0) {
        clock = fallback;
    }
    uint64_t *remainder =
        kind == WORKER_WQR ? &machine->wqr_clock_remainder : &machine->motor_clock_remainder;
    uint32_t cycles = convert_base_cycles(base_cycles, clock, remainder);
    return kind == WORKER_WQR ? run_kinetis(machine, device, &machine->wqr_instructions,
                                            &machine->wqr_raw_instructions, cycles, "WQR")
                              : run_motor(machine, cycles);
}

static unsigned __stdcall mcu_worker(void *context) {
    McuWorker *worker = context;
    for (;;) {
        WaitForSingleObject(worker->start, INFINITE);
        if (worker->stop) {
            return 0;
        }
        worker->result = run_worker(worker->machine, worker->kind, worker->cycles);
        SetEvent(worker->done);
    }
}

static bool start_workers(Dd1SimMachine *machine) {
    for (size_t index = 0; index < WORKER_COUNT; ++index) {
        McuWorker *worker = &machine->workers[index];
        worker->machine = machine;
        worker->kind = (WorkerKind)index;
        worker->start = CreateEventW(NULL, FALSE, FALSE, NULL);
        worker->done = CreateEventW(NULL, FALSE, FALSE, NULL);
        uintptr_t thread = worker->start != NULL && worker->done != NULL
                               ? _beginthreadex(NULL, 0, mcu_worker, worker, 0, NULL)
                               : 0;
        worker->thread = (HANDLE)thread;
        if (worker->thread == NULL) {
            return false;
        }
        SetThreadPriority(worker->thread, THREAD_PRIORITY_BELOW_NORMAL);
    }
    return true;
}

static bool run_quantum(Dd1SimMachine *machine, uint32_t cycles) {
    feed_links(machine);
    update_motor_encoder(machine);
    if (machine->parallel) {
        HANDLE done[WORKER_COUNT];
        for (size_t index = 0; index < WORKER_COUNT; ++index) {
            machine->workers[index].cycles = cycles;
            done[index] = machine->workers[index].done;
            SetEvent(machine->workers[index].start);
        }
        if (WaitForMultipleObjects(WORKER_COUNT, done, TRUE, INFINITE) == WAIT_FAILED) {
            snprintf(machine->status, sizeof(machine->status), "MCU worker synchronization failed");
            return false;
        }
        for (size_t index = 0; index < WORKER_COUNT; ++index) {
            if (!machine->workers[index].result) {
                return false;
            }
        }
    } else {
        for (size_t index = 0; index < WORKER_COUNT; ++index) {
            if (!run_worker(machine, (WorkerKind)index, cycles)) {
                return false;
            }
        }
    }
    capture_display(machine);
    collect_base_links(machine);
    collect_wqr_links(machine);
    collect_motor_link(machine);
    return machine->running;
}

bool dd1_sim_machine_run(Dd1SimMachine *machine, uint32_t instruction_rounds) {
    if (machine == NULL || !machine->running) {
        return false;
    }
    if (machine->parallel && machine->workers[0].thread == NULL && !start_workers(machine)) {
        machine->running = false;
        snprintf(machine->status, sizeof(machine->status), "could not start MCU workers");
        return false;
    }
    while (instruction_rounds != 0) {
        uint32_t quantum =
            machine->communication_guard != 0 ? COMMUNICATION_QUANTUM_CYCLES : SYNC_QUANTUM_CYCLES;
        if (machine->communication_guard != 0) {
            --machine->communication_guard;
        }
        uint32_t cycles = instruction_rounds < quantum ? instruction_rounds : quantum;
        if (!run_quantum(machine, cycles)) {
            machine->running = false;
            return false;
        }
        instruction_rounds -= cycles;
    }
    snprintf(machine->status, sizeof(machine->status), "Running official firmware");
    return true;
}

void dd1_sim_machine_set_parallel(Dd1SimMachine *machine, bool parallel) {
    if (machine != NULL) {
        machine->parallel = parallel;
    }
}

void dd1_sim_machine_set_inputs(Dd1SimMachine *machine, int16_t angle_tenths, uint32_t buttons) {
    if (machine == NULL) {
        return;
    }
    machine->angle_tenths = angle_tenths;
    machine->buttons = buttons & UINT32_C(0x00ffffff);
    dd1_sim_wheel_set_buttons(&machine->wheel, machine->buttons);
}

void dd1_sim_machine_snapshot(const Dd1SimMachine *machine, Dd1SimSnapshot *snapshot) {
    if (snapshot == NULL) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    if (machine == NULL) {
        return;
    }
    memcpy(snapshot->display, machine->display, sizeof(snapshot->display));
    memcpy(snapshot->wheel_output, machine->wheel.output, sizeof(snapshot->wheel_output));
    snapshot->base_instructions = machine->base_instructions;
    snapshot->wqr_instructions = machine->wqr_instructions;
    snapshot->motor_instructions = machine->motor_instructions;
    snapshot->base_program_counter = dspic33_get_program_counter(machine->base);
    snapshot->wqr_program_counter = cortex_m4_get_register(kinetis_cpu(machine->wqr), 15);
    snapshot->motor_program_counter = cortex_m4_get_register(kinetis_cpu(machine->motor), 15);
    snapshot->base_clock_hz = BASE_CLOCK_HZ;
    snapshot->wqr_clock_hz = kinetis_core_clock_hz(machine->wqr);
    snapshot->motor_clock_hz = kinetis_core_clock_hz(machine->motor);
    snapshot->display_bytes = machine->display_bytes;
    snapshot->base_uart_bytes = machine->base_uart_bytes;
    snapshot->wqr_uart_bytes = machine->wqr_uart_bytes;
    snapshot->base_motor_words = machine->base_motor_words;
    snapshot->motor_base_words = machine->motor_base_words;
    snapshot->wheel_exchanges = machine->wheel.exchanges;
    snapshot->buttons = machine->buttons;
    snapshot->motor_position = machine->motor_position;
    snapshot->motor_current = machine->motor_current;
    snapshot->angle_tenths = machine->angle_tenths;
    snapshot->display_ready = machine->display_ready;
    snapshot->running = machine->running;
    memcpy(snapshot->status, machine->status, sizeof(snapshot->status));
}
