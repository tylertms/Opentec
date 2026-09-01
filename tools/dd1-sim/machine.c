#include "machine.h"

#include <cortex_m4_firmware_image.h>
#include <dspic33.h>
#include <dspic33_firmware_image.h>
#include <kinetis.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wheel.h"

enum {
    LINK_QUEUE_CAPACITY = 4096,
    BASE_CLOCK_HZ = 60000000,
    WQR_CLOCK_HZ = 96000000,
    MOTOR_CLOCK_HZ = 71991296,
    BASE_UART_CHANNEL = 2,
    BASE_MOTOR_SPI_CHANNEL = 0,
    MOTOR_FRAME_SIZE = 13,
    BASE_NVM_CONTROL = 0x0728,
    BASE_INTERRUPT_CONTROL_2 = 0x08c2,
    BASE_MOTOR_SPI_STATUS = 0x0240,
    BASE_NVM_WRITE = 0x8000,
    BASE_PERIPHERAL_ENABLE = 0x8000,
    BASE_GLOBAL_INTERRUPT_ENABLE = 0x8000,
    BASE_NVM_WRITE_OPCODE = 0xa8e729,
    MOTOR_FTM2_COUNTER = 0x4003a004,
    MOTOR_FTM2_MODULUS = 0x4003a008,
    MOTOR_INDEX_TRIGGER_INSTRUCTIONS = 500000,
};

typedef struct {
    uint16_t values[LINK_QUEUE_CAPACITY];
    size_t head;
    size_t count;
} LinkQueue;

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
    uint64_t base_instructions;
    uint64_t wqr_instructions;
    uint64_t motor_instructions;
    uint64_t display_bytes;
    uint64_t base_uart_bytes;
    uint64_t wqr_uart_bytes;
    uint64_t base_motor_words;
    uint64_t motor_base_words;
    uint32_t buttons;
    int32_t motor_position;
    int16_t motor_current;
    int16_t angle_tenths;
    uint8_t motor_index_stage;
    uint16_t base_saved_interrupt_control;
    bool base_nvm_workaround;
    bool display_ready;
    bool running;
    char status[DD1_SIM_STATUS_SIZE];
};

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

static bool load_base(Dd1SimMachine *machine, const char *path, char *error, size_t error_size) {
    FirmwareImage image;
    uint32_t reset_address = 0;
    char image_error[256] = {0};

    machine->base = dspic33_create_for_device(DSPIC33EP_MU_DEVICE_512MU810);
    if (machine->base == NULL) {
        write_error(error, error_size, "could not create the base MCU");
        return false;
    }
    if (!firmware_image_open(&image, path, image_error, sizeof(image_error))) {
        write_error(error, error_size, "could not open base firmware: %s", image_error);
        return false;
    }
    bool loaded =
        firmware_image_load_program(&image, machine->base, image_error, sizeof(image_error)) &&
        firmware_image_symbol(&image, "__reset", &reset_address, image_error, sizeof(image_error));
    firmware_image_close(&image);
    if (!loaded) {
        write_error(error, error_size, "could not load base firmware: %s", image_error);
        return false;
    }
    dspic33_reset(machine->base, reset_address);
    dspic33_gpio_drive(machine->base, 1, UINT16_C(0x0002), UINT16_C(0x0006));
    dspic33_gpio_drive(machine->base, 3, UINT16_C(0x0002), UINT16_C(0x3202));
    dspic33_gpio_drive(machine->base, 4, 0, UINT16_C(0x0300));
    return true;
}

static bool load_wqr(Dd1SimMachine *machine, const char *path, char *error, size_t error_size) {
    KinetisConfiguration configuration = kinetis_configuration(KINETIS_PROFILE_MKV30F12810);
    configuration.package = KINETIS_PACKAGE_LH_64_LQFP;
    configuration.vector_table_address = 0xa000U;
    machine->wqr = kinetis_create(configuration);
    if (machine->wqr == NULL || !cortex_m4_load_elf(machine->wqr, path, NULL) ||
        !kinetis_reset(machine->wqr)) {
        write_error(error, error_size, "could not load WQR firmware");
        return false;
    }
    kinetis_gpio_drive(machine->wqr, 0, 4, true);
    kinetis_gpio_drive(machine->wqr, 0, 18, true);
    kinetis_gpio_drive(machine->wqr, 0, 19, true);
    kinetis_gpio_drive(machine->wqr, 2, 2, false);
    kinetis_set_adc_input(machine->wqr, 0, KINETIS_ADC_MUX_A, 12, 2048);
    return true;
}

static bool load_motor(Dd1SimMachine *machine, const char *path, char *error, size_t error_size) {
    KinetisConfiguration configuration = kinetis_configuration(KINETIS_PROFILE_MKV10Z1287);
    configuration.package = KINETIS_PACKAGE_LH_64_LQFP;
    configuration.vector_table_address = 0xa000U;
    machine->motor = kinetis_create(configuration);
    if (machine->motor == NULL || !cortex_m4_load_elf(machine->motor, path, NULL) ||
        !kinetis_reset(machine->motor)) {
        write_error(error, error_size, "could not load motor firmware");
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
    kinetis_gpio_drive(machine->motor, 4, 24, true);
    kinetis_set_ftm_input(machine->motor, 1, 1, false);
    kinetis_set_ftm_input(machine->motor, 2, 0, false);
    kinetis_set_ftm_input(machine->motor, 2, 1, false);
    kinetis_set_ftm_quadrature_input(machine->motor, 2, 0, false);
    kinetis_set_ftm_quadrature_input(machine->motor, 2, 1, false);
    return true;
}

Dd1SimMachine *dd1_sim_machine_create(const char *base_path, const char *wqr_path,
                                      const char *motor_path, char *error, size_t error_size) {
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
    snprintf(machine->status, sizeof(machine->status), "Booting DD1 firmware");
    return machine;
}

void dd1_sim_machine_destroy(Dd1SimMachine *machine) {
    if (machine == NULL) {
        return;
    }
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

static bool step_base(Dd1SimMachine *machine) {
    uint32_t program_counter = dspic33_get_program_counter(machine->base);
    uint32_t opcode = dspic33_read_program_word(machine->base, program_counter);
    if (!machine->base_nvm_workaround && opcode == BASE_NVM_WRITE_OPCODE) {
        machine->base_saved_interrupt_control =
            dspic33_read_word(machine->base, BASE_INTERRUPT_CONTROL_2);
        dspic33_write_word(machine->base, BASE_INTERRUPT_CONTROL_2,
                           machine->base_saved_interrupt_control & ~BASE_GLOBAL_INTERRUPT_ENABLE);
        machine->base_nvm_workaround = true;
    }
    Dspic33StopReason stop = dspic33_step(machine->base);
    if (machine->base_nvm_workaround &&
        (dspic33_read_word(machine->base, BASE_NVM_CONTROL) & BASE_NVM_WRITE) == 0) {
        dspic33_write_word(machine->base, BASE_INTERRUPT_CONTROL_2,
                           machine->base_saved_interrupt_control);
        machine->base_nvm_workaround = false;
    }
    machine->base_instructions = dspic33_get_instruction_count(machine->base);
    if (stop == DSPIC33_RUNNING || stop == DSPIC33_SLEEPING || stop == DSPIC33_IDLING) {
        return true;
    }
    snprintf(machine->status, sizeof(machine->status), "Base stopped: %s at 0x%08x",
             dspic33_stop_reason_name(stop), dspic33_get_program_counter(machine->base));
    return false;
}

static bool step_kinetis(Kinetis *device, uint64_t *instructions, const char *name, char *status,
                         size_t status_size) {
    CortexM4Result result = cortex_m4_step(kinetis_cpu(device));
    *instructions = cortex_m4_get_instruction_count(kinetis_cpu(device));
    if (result.stop == CORTEX_M4_STOP_RUNNING || result.stop == CORTEX_M4_STOP_CLOCK) {
        return true;
    }
    snprintf(status, status_size, "%s stopped: %u at 0x%08x", name, (unsigned)result.stop,
             result.pc);
    return false;
}

static bool step_next_processor(Dd1SimMachine *machine) {
    double base_time = (double)dspic33_get_cycle_count(machine->base) / BASE_CLOCK_HZ;
    double wqr_time = (double)cortex_m4_get_cycle_count(kinetis_cpu(machine->wqr)) / WQR_CLOCK_HZ;
    double motor_time =
        (double)cortex_m4_get_cycle_count(kinetis_cpu(machine->motor)) / MOTOR_CLOCK_HZ;
    if (base_time <= wqr_time && base_time <= motor_time) {
        return step_base(machine);
    }
    if (wqr_time <= motor_time) {
        return step_kinetis(machine->wqr, &machine->wqr_instructions, "WQR", machine->status,
                            sizeof(machine->status));
    }
    return step_kinetis(machine->motor, &machine->motor_instructions, "Motor", machine->status,
                        sizeof(machine->status));
}

static void capture_display(Dd1SimMachine *machine) {
    Dspic33PmpTransfer transfer;
    while (dspic33_pmp_transmit(machine->base, &transfer)) {
        ++machine->display_bytes;
        bool data_mode = false;
        if (!dspic33_gpio_pin(machine->base, 3, 3, &data_mode) || !data_mode) {
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

static void collect_base_links(Dd1SimMachine *machine) {
    Dspic33UartFrame frame;
    uint8_t value;
    while (dspic33_uart_transmit(machine->base, BASE_UART_CHANNEL, &frame)) {
        ++machine->base_uart_bytes;
        if (!queue_push(&machine->base_to_wqr, (uint8_t)frame.value)) {
            break;
        }
    }
    while (dspic33_spi_transmit(machine->base, BASE_MOTOR_SPI_CHANNEL, &value)) {
        ++machine->base_motor_words;
        if (!queue_push(&machine->base_to_motor, value)) {
            break;
        }
    }
}

static void collect_wqr_links(Dd1SimMachine *machine) {
    uint8_t value;
    KinetisSpiTransfer transfer;
    while (kinetis_uart1_transmit(machine->wqr, &value)) {
        ++machine->wqr_uart_bytes;
        if (!queue_push(&machine->wqr_to_base, value)) {
            break;
        }
    }
    while (kinetis_spi_transfer(machine->wqr, KINETIS_SERIAL_SPI0, &transfer)) {
        machine->wheel_output[machine->wheel_output_index++] = (uint8_t)transfer.data;
        if (machine->wheel_output_index == DD1_SIM_WHEEL_OUTPUT_SIZE) {
            dd1_sim_wheel_accept_output(&machine->wheel, machine->wheel_output);
            dd1_sim_wheel_response(&machine->wheel, machine->wheel_response);
            machine->wheel_output_index = 0;
            machine->wheel_response_index = 0;
        }
    }
}

static void decode_motor_frame(Dd1SimMachine *machine) {
    if (machine->motor_frame[0] != 0x7b || machine->motor_frame[12] != 0x7d ||
        (machine->motor_frame[1] & 0x7fU) != 1) {
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
    while (kinetis_spi_transfer(machine->motor, KINETIS_SERIAL_SPI0, &transfer)) {
        ++machine->motor_base_words;
        if (!queue_push(&machine->motor_to_base, transfer.data)) {
            break;
        }
        machine->motor_frame[machine->motor_frame_index++] = (uint8_t)transfer.data;
        if (machine->motor_frame_index == MOTOR_FRAME_SIZE) {
            decode_motor_frame(machine);
            machine->motor_frame_index = 0;
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
           machine->motor_to_base.count != 0 &&
           dspic33_spi_receive(machine->base, BASE_MOTOR_SPI_CHANNEL,
                               queue_front(&machine->motor_to_base), 0)) {
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
    uint32_t counter = (uint32_t)(bounded_angle + 4500) * modulus / 9000U;
    kinetis_write(machine->motor, MOTOR_FTM2_COUNTER, &counter, sizeof(counter));
    if (machine->motor_instructions >= MOTOR_INDEX_TRIGGER_INSTRUCTIONS &&
        machine->motor_index_stage == 0) {
        kinetis_gpio_drive(machine->motor, 4, 24, false);
        machine->motor_index_stage = 1;
    } else if (machine->motor_index_stage == 1) {
        kinetis_gpio_drive(machine->motor, 4, 24, true);
        machine->motor_index_stage = 2;
    }
}

bool dd1_sim_machine_run(Dd1SimMachine *machine, uint32_t instruction_rounds) {
    if (machine == NULL || !machine->running) {
        return false;
    }
    for (uint32_t round = 0; round < instruction_rounds; ++round) {
        for (uint8_t processor = 0; processor < 3; ++processor) {
            feed_links(machine);
            if (!step_next_processor(machine)) {
                machine->running = false;
                return false;
            }
            capture_display(machine);
            collect_base_links(machine);
            collect_wqr_links(machine);
            collect_motor_link(machine);
        }
        if ((round & 0xffU) == 0) {
            update_motor_encoder(machine);
        }
    }
    snprintf(machine->status, sizeof(machine->status),
             "Running  base %.2fM  WQR %.2fM  motor %.2fM instructions",
             (double)machine->base_instructions / 1000000.0,
             (double)machine->wqr_instructions / 1000000.0,
             (double)machine->motor_instructions / 1000000.0);
    return true;
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
