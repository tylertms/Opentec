#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cortex_m4_firmware_image.h"

enum {
    FIRMWARE_EFFECT_CONSTANT = 8,
    FIRMWARE_EFFECT_WINDOW = 11,
    FIRMWARE_EFFECT_DIRECTIONAL = 12,
    FIRMWARE_POSITION_EFFECT_SLOT = 16,
};

typedef struct {
    uint8_t type;
    uint8_t active;
    uint8_t padding[2];
    uint8_t data[16];
} FirmwareEffect;

typedef struct {
    uint8_t settings[12];
    FirmwareEffect effects[20];
} FirmwareEffectEngine;

static const uint8_t force_frame[] = {
    0x7bU, 0x01U, 0x05U, 0x06U, 0x07U, 0x08U, 0xcdU, 0xabU, 0x68U, 0x24U, 0xfeU, 0xe9U, 0x7dU,
};

static const uint8_t steering_range_request[] = {32U, 0x82U};
static const uint8_t encoder_direction_request[] = {5U, 0xcdU, 0xabU};
static const uint8_t encoder_direction_complete_response[] = {0U, 0U, 0U, 0U, 2U};

static const uint8_t effect_frames[][13] = {
    {0x7bU, 0x02U, 0x03U, 0x01U, 0x08U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x31U, 0xbbU, 0x7dU},
    {0x7bU, 0x02U, 0x03U, 0x11U, 0x0bU, 0x80U, 0x80U, 0x44U, 0x00U, 0x95U, 0x33U, 0xafU, 0x7dU},
    {0x7bU, 0x02U, 0x03U, 0x21U, 0x0cU, 0x01U, 0x00U, 0x01U, 0x00U, 0xa0U, 0xedU, 0xfbU, 0x7dU},
    {0x7bU, 0x02U, 0x05U, 0x21U, 0x08U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x0cU, 0x55U, 0x7dU},
    {0x7bU, 0x02U, 0x0bU, 0x21U, 0x08U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x1dU, 0xcbU, 0x7dU},
    {0x7bU, 0x02U, 0x43U, 0x21U, 0x08U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x17U, 0x23U, 0x7dU},
    {0x7bU, 0x02U, 0x23U, 0x21U, 0x08U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xafU, 0xaeU, 0x7dU},
    {0x7bU, 0x02U, 0x03U, 0x23U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x66U, 0xb8U, 0x7dU},
    {0x7bU, 0x02U, 0x03U, 0x05U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xb5U, 0x77U, 0x7dU},
    {0x7bU, 0x02U, 0x03U, 0x04U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xd4U, 0xcfU, 0x7dU},
};

static bool execution_valid(Kinetis *device) {
    const CortexM4 *cpu = kinetis_cpu(device);
    const uint32_t fault_status = cortex_m4_get_fault_status(cpu);
    const uint32_t exception = cortex_m4_get_xpsr(cpu) & 0x1ffU;
    if (fault_status != 0U || exception == 3U)
        fprintf(stderr, "invalid execution: cfsr=0x%08" PRIx32 " exception=%" PRIu32 "\n",
                fault_status, exception);
    return fault_status == 0U && exception != 3U;
}

static bool run_phase(Kinetis *device, uint64_t instruction_limit) {
    const CortexM4Result result = cortex_m4_run(
        kinetis_cpu(device), (CortexM4RunLimits){.instruction_limit = instruction_limit,
                                                 .cycle_limit = instruction_limit * 20U});
    const bool valid = result.stop == CORTEX_M4_STOP_LIMIT && execution_valid(device);
    if (!valid)
        fprintf(stderr, "phase stopped: stop=%u pc=0x%08" PRIx32 "\n", result.stop, result.pc);
    return valid;
}

static bool run_to_symbol(Kinetis *device, const char *path, const char *name,
                          uint64_t instruction_limit) {
    uint32_t address = 0U;
    if (!cortex_m4_elf_symbol(path, name, &address) ||
        !cortex_m4_set_breakpoint(kinetis_cpu(device), 0U, address & ~1U, true)) {
        return false;
    }
    const CortexM4Result result = cortex_m4_run(
        kinetis_cpu(device), (CortexM4RunLimits){.instruction_limit = instruction_limit,
                                                 .cycle_limit = instruction_limit * 20U});
    const bool stopped = result.stop == CORTEX_M4_STOP_BREAKPOINT && execution_valid(device);
    const bool disabled = cortex_m4_set_breakpoint(kinetis_cpu(device), 0U, address & ~1U, false);
    if (!stopped)
        fprintf(stderr, "did not reach %s: stop=%u pc=0x%08" PRIx32 " exception=%" PRIu32 "\n",
                name, result.stop, result.pc, cortex_m4_get_xpsr(kinetis_cpu(device)) & 0x1ffU);
    return disabled && stopped;
}

static bool finish_interrupt(Kinetis *device, uint64_t instruction_limit) {
    const uint32_t active_exception = cortex_m4_get_xpsr(kinetis_cpu(device)) & 0x1ffU;
    for (uint64_t instruction = 0U; instruction < instruction_limit; ++instruction) {
        const CortexM4Result result = cortex_m4_step(kinetis_cpu(device));
        if (result.stop != CORTEX_M4_STOP_RUNNING || !execution_valid(device))
            return false;
        if ((cortex_m4_get_xpsr(kinetis_cpu(device)) & 0x1ffU) != active_exception)
            return true;
    }
    return false;
}

static bool run_until_index_interrupt_armed(Kinetis *device) {
    for (uint32_t phase = 0U; phase < 100U; ++phase) {
        uint32_t pin_control = 0U;
        if (!kinetis_read(device, UINT32_C(0x4004d060), &pin_control, sizeof(pin_control)))
            return false;
        if ((pin_control & UINT32_C(0x000f0000)) == UINT32_C(0x000a0000))
            return true;
        if (!run_phase(device, 1000000U))
            return false;
    }
    return false;
}

static bool run_until_encoder_running(Kinetis *device) {
    for (uint32_t phase = 0U; phase < 100U; ++phase) {
        uint32_t status = 0U;
        if (!kinetis_read(device, UINT32_C(0x4003a000), &status, sizeof(status)))
            return false;
        if ((status & (1UL << 6U)) != 0U)
            return true;
        if (!run_phase(device, 1000000U))
            return false;
    }
    return false;
}

static bool configure_inputs(Kinetis *device) {
    const uint8_t adc0_channels[] = {4U, 7U, 9U};
    const uint8_t adc1_channels[] = {0U, 2U, 10U};
    for (size_t index = 0U; index < sizeof(adc0_channels); ++index) {
        if (!kinetis_set_adc_input(device, 0U, KINETIS_ADC_MUX_A, adc0_channels[index], 2048U))
            return false;
    }
    for (size_t index = 0U; index < sizeof(adc1_channels); ++index) {
        if (!kinetis_set_adc_input(device, 1U, KINETIS_ADC_MUX_A, adc1_channels[index], 2048U))
            return false;
    }
    return kinetis_gpio_drive(device, 4U, 24U, true) &&
           kinetis_set_ftm_input(device, 1U, 1U, false) &&
           kinetis_set_ftm_input(device, 2U, 0U, false) &&
           kinetis_set_ftm_input(device, 2U, 1U, false) &&
           kinetis_set_ftm_quadrature_input(device, 2U, 0U, false) &&
           kinetis_set_ftm_quadrature_input(device, 2U, 1U, false);
}

static bool adc_calibration_configuration_matches(Kinetis *device) {
    uint8_t adc0_configuration = 0U;
    uint8_t adc1_configuration = 0U;
    const bool readable =
        kinetis_read(device, UINT32_C(0x4003b008), &adc0_configuration,
                     sizeof(adc0_configuration)) &&
        kinetis_read(device, UINT32_C(0x4003c008), &adc1_configuration, sizeof(adc1_configuration));
    if (!readable || adc0_configuration != UINT8_C(0x24) || adc1_configuration != UINT8_C(0x24)) {
        fprintf(stderr, "ADC calibration CFG1: adc0=0x%02" PRIx8 " adc1=0x%02" PRIx8 "\n",
                adc0_configuration, adc1_configuration);
        return false;
    }
    return true;
}

static bool startup_output_state_safe(Kinetis *device) {
    uint32_t output_mask = 0U;
    uint32_t mode = 0U;
    uint32_t combine = 0U;
    uint32_t deadtime = 0U;
    uint32_t modulus = 0U;
    uint32_t ftm2_configuration = 0U;
    uint32_t ftm2_synchronization = 0U;
    uint32_t ftm2_filter = 0U;
    uint32_t ftm2_status = 0U;
    uint32_t spi_input_control = 0U;
    const bool readable =
        kinetis_read(device, UINT32_C(0x40038060), &output_mask, sizeof(output_mask)) &&
        kinetis_read(device, UINT32_C(0x40038054), &mode, sizeof(mode)) &&
        kinetis_read(device, UINT32_C(0x40038064), &combine, sizeof(combine)) &&
        kinetis_read(device, UINT32_C(0x40038068), &deadtime, sizeof(deadtime)) &&
        kinetis_read(device, UINT32_C(0x40038008), &modulus, sizeof(modulus)) &&
        kinetis_read(device, UINT32_C(0x4003a084), &ftm2_configuration,
                     sizeof(ftm2_configuration)) &&
        kinetis_read(device, UINT32_C(0x4003a08c), &ftm2_synchronization,
                     sizeof(ftm2_synchronization)) &&
        kinetis_read(device, UINT32_C(0x4003a078), &ftm2_filter, sizeof(ftm2_filter)) &&
        kinetis_read(device, UINT32_C(0x4003a000), &ftm2_status, sizeof(ftm2_status)) &&
        kinetis_read(device, UINT32_C(0x4004c01c), &spi_input_control, sizeof(spi_input_control));
    if (!readable || output_mask != UINT32_C(0x3f) || (mode & UINT32_C(0x60)) != UINT32_C(0x40) ||
        (combine & UINT32_C(0x737373)) != UINT32_C(0x737373) ||
        (deadtime & UINT32_C(0x3f)) != 50U || modulus != 2249U ||
        ftm2_configuration != UINT32_C(0xc0) || ftm2_synchronization != 0U || ftm2_filter != 0U ||
        ftm2_status != 8U || (spi_input_control & UINT32_C(3)) != UINT32_C(2)) {
        fprintf(stderr,
                "startup registers: readable=%u outmask=0x%08" PRIx32 " mode=0x%08" PRIx32
                " combine=0x%08" PRIx32 " deadtime=0x%08" PRIx32 " mod=%" PRIu32
                " ftm2-conf=0x%08" PRIx32 " ftm2-synconf=0x%08" PRIx32 " ftm2-filter=0x%08" PRIx32
                " ftm2-sc=0x%08" PRIx32 " spi-input=0x%08" PRIx32 "\n",
                readable, output_mask, mode, combine, deadtime, modulus, ftm2_configuration,
                ftm2_synchronization, ftm2_filter, ftm2_status, spi_input_control);
        return false;
    }
    for (uint32_t channel = 0U; channel < 6U; ++channel) {
        uint32_t channel_control = 0U;
        if (!kinetis_read(device, UINT32_C(0x4003800c) + channel * 8U, &channel_control,
                          sizeof(channel_control)) ||
            (channel_control & UINT32_C(0x0c)) != UINT32_C(0x08)) {
            fprintf(stderr, "PWM channel %" PRIu32 " control: 0x%08" PRIx32 "\n", channel,
                    channel_control);
            return false;
        }
    }
    return true;
}

static bool call_symbol(Kinetis *device, const char *path, const char *name) {
    uint32_t address = 0U;
    uint32_t registers[15];
    for (uint8_t index = 0U; index < 15U; ++index)
        registers[index] = cortex_m4_get_register(kinetis_cpu(device), index);
    const uint32_t return_address = cortex_m4_get_register(kinetis_cpu(device), 15U) & ~1U;
    if (!cortex_m4_elf_symbol(path, name, &address) ||
        !cortex_m4_set_breakpoint(kinetis_cpu(device), 1U, return_address, true))
        return false;
    cortex_m4_set_register(kinetis_cpu(device), 14U, return_address | 1U);
    cortex_m4_set_register(kinetis_cpu(device), 15U, address & ~1U);
    const CortexM4Result result =
        cortex_m4_run(kinetis_cpu(device),
                      (CortexM4RunLimits){.instruction_limit = 1000U, .cycle_limit = 20000U});
    const bool returned = result.stop == CORTEX_M4_STOP_BREAKPOINT && execution_valid(device);
    const bool disabled = cortex_m4_set_breakpoint(kinetis_cpu(device), 1U, return_address, false);
    for (uint8_t index = 0U; index < 15U; ++index)
        cortex_m4_set_register(kinetis_cpu(device), index, registers[index]);
    return returned && disabled;
}

static bool ftm2_reinitialization_matches(Kinetis *device, const char *path) {
    const uint32_t dirty_status = UINT32_C(0xe7);
    const uint32_t dirty_filter = UINT32_MAX;
    if (!kinetis_write(device, UINT32_C(0x4003a000), &dirty_status, sizeof(dirty_status)) ||
        !kinetis_write(device, UINT32_C(0x4003a078), &dirty_filter, sizeof(dirty_filter)))
        return false;
    CortexM4 *cpu = kinetis_cpu(device);
    cortex_m4_set_register(cpu, 0U, 2249U);
    cortex_m4_set_register(cpu, 1U, 0U);
    cortex_m4_set_register(cpu, 2U, 0U);
    cortex_m4_set_register(cpu, 3U, 0U);
    if (!call_symbol(device, path, "motor_tick_timer_initialize"))
        return false;
    uint32_t status = 0U;
    uint32_t filter = 0U;
    return kinetis_read(device, UINT32_C(0x4003a000), &status, sizeof(status)) &&
           kinetis_read(device, UINT32_C(0x4003a078), &filter, sizeof(filter)) && status == 8U &&
           filter == 0U;
}

static bool pdb_status_clear_matches(Kinetis *device, const char *path) {
    uint32_t initial_channel_zero = 0U;
    uint32_t initial_channel_one = 0U;
    if (!kinetis_read(device, UINT32_C(0x40036014), &initial_channel_zero,
                      sizeof(initial_channel_zero)) ||
        !kinetis_read(device, UINT32_C(0x4003603c), &initial_channel_one,
                      sizeof(initial_channel_one)) ||
        (initial_channel_zero & UINT32_C(0xff0000)) == 0U ||
        (initial_channel_one & UINT32_C(0xff0000)) == 0U)
        return false;
    if (!call_symbol(device, path, "PDB0_PDB1_IRQHandler"))
        return false;
    uint32_t channel_zero = 0U;
    uint32_t channel_one = 0U;
    return kinetis_read(device, UINT32_C(0x40036014), &channel_zero, sizeof(channel_zero)) &&
           kinetis_read(device, UINT32_C(0x4003603c), &channel_one, sizeof(channel_one)) &&
           (channel_zero & UINT32_C(0xff00ff)) == 0U && (channel_one & UINT32_C(0xff00ff)) == 0U;
}

static bool fatal_output_state_safe(Kinetis *device, const char *path) {
    uint32_t fault_address = 0U;
    if (!cortex_m4_elf_symbol(path, "WDOG_EWM_IRQHandler", &fault_address))
        return false;
    cortex_m4_set_register(kinetis_cpu(device), 15U, fault_address & ~1U);
    for (uint32_t instruction = 0U; instruction < 64U; ++instruction) {
        if (cortex_m4_step(kinetis_cpu(device)).stop != CORTEX_M4_STOP_RUNNING)
            return false;
        uint32_t output_mask = 0U;
        uint32_t port_c_output = 0U;
        if (kinetis_read(device, UINT32_C(0x40038060), &output_mask, sizeof(output_mask)) &&
            kinetis_read(device, UINT32_C(0x400ff080), &port_c_output, sizeof(port_c_output)) &&
            output_mask == UINT32_C(0x3f) && (port_c_output & (1UL << 1U)) == 0U)
            return true;
    }
    return false;
}

static bool queue_frame(Kinetis *device, const uint8_t *frame, size_t size) {
    for (size_t index = 0U; index < size; ++index) {
        if (!kinetis_serial_receive(device, KINETIS_SERIAL_SPI0, frame[index], 0U))
            return false;
    }
    return true;
}

static int32_t effect_int32(const FirmwareEffect *effect, size_t offset) {
    int32_t value = 0;
    memcpy(&value, effect->data + offset, sizeof(value));
    return value;
}

static uint16_t effect_uint16(const FirmwareEffect *effect, size_t offset) {
    uint16_t value = 0U;
    memcpy(&value, effect->data + offset, sizeof(value));
    return value;
}

static bool effect_state_matches(size_t index, const FirmwareEffectEngine *engine) {
    const FirmwareEffect *effect = &engine->effects[index < 2U ? index : 2U];
    if (index == 0U)
        return effect->active && effect->type == FIRMWARE_EFFECT_CONSTANT &&
               effect_int32(effect, 0U) == 65535;
    if (index == 1U)
        return effect->active && effect->type == FIRMWARE_EFFECT_WINDOW &&
               effect_int32(effect, 0U) == 0 && effect_int32(effect, 4U) == 0 &&
               effect->data[8] == 4U && effect->data[9] == 4U && (int8_t)effect->data[10] == -1 &&
               (int8_t)effect->data[11] == -1 && effect_uint16(effect, 12U) == UINT16_C(0x9595);
    if (index == 2U)
        return effect->active && effect->type == FIRMWARE_EFFECT_DIRECTIONAL &&
               effect_uint16(effect, 0U) == 1U && effect_uint16(effect, 2U) == 1U &&
               (int8_t)effect->data[4] == -1 && (int8_t)effect->data[5] == -1 &&
               effect_uint16(effect, 6U) == UINT16_C(0xa0a0);
    if (index < 7U)
        return effect->active && effect->type == FIRMWARE_EFFECT_CONSTANT &&
               effect_int32(effect, 0U) == 65535;
    if (index == 7U)
        return !effect->active;
    return engine->effects[FIRMWARE_POSITION_EFFECT_SLOT].active == (index == 9U);
}

static bool queue_effect_frames(Kinetis *device) {
    for (size_t index = 0U; index < sizeof(effect_frames) / sizeof(effect_frames[0]); ++index) {
        if (!queue_frame(device, effect_frames[index], sizeof(effect_frames[index])))
            return false;
    }
    return true;
}

static bool apply_effect_frame(Kinetis *device, const char *path, size_t index) {
    if (!run_to_symbol(device, path, "motor_force_feedback_command_apply", 10000000U))
        return false;
    const uint32_t engine_address = cortex_m4_get_register(kinetis_cpu(device), 0U);
    const uint32_t command_address = cortex_m4_get_register(kinetis_cpu(device), 1U);
    const uint32_t return_address = cortex_m4_get_register(kinetis_cpu(device), 14U) & ~1U;
    uint8_t command[7] = {0};
    if (!kinetis_read(device, command_address, command, sizeof(command)) ||
        memcmp(command, effect_frames[index] + 3U, sizeof(command)) != 0 ||
        !cortex_m4_set_breakpoint(kinetis_cpu(device), 1U, return_address, true))
        return false;
    const CortexM4Result result =
        cortex_m4_run(kinetis_cpu(device),
                      (CortexM4RunLimits){.instruction_limit = 100000U, .cycle_limit = 2000000U});
    const bool returned = result.stop == CORTEX_M4_STOP_BREAKPOINT && execution_valid(device) &&
                          cortex_m4_get_register(kinetis_cpu(device), 0U) != 0U;
    const bool disabled = cortex_m4_set_breakpoint(kinetis_cpu(device), 1U, return_address, false);
    FirmwareEffectEngine engine = {0};
    const bool readable = kinetis_read(device, engine_address, &engine, sizeof(engine));
    if (!returned || !disabled || !readable || !effect_state_matches(index, &engine)) {
        const FirmwareEffect *effect = &engine.effects[index < 2U ? index : 2U];
        fprintf(stderr,
                "effect frame %zu did not apply: returned=%u disabled=%u readable=%u type=%u "
                "active=%u value=%" PRId32 "\n",
                index, returned, disabled, readable, effect->type, effect->active,
                effect_int32(effect, 0U));
        return false;
    }
    return true;
}

static bool apply_effect_frames(Kinetis *device, const char *path) {
    for (size_t index = 0U; index < sizeof(effect_frames) / sizeof(effect_frames[0]); ++index) {
        if (!apply_effect_frame(device, path, index))
            return false;
    }
    return true;
}

static bool received_force_frame(const char *path, const Kinetis *device) {
    uint32_t transfer_buffers_symbol = 0U;
    uint32_t transfer_buffers_address = 0U;
    uint8_t received[sizeof(force_frame)] = {0};
    const bool readable =
        cortex_m4_elf_symbol(path, "transfer_buffers", &transfer_buffers_symbol) &&
        kinetis_read(device, transfer_buffers_symbol, &transfer_buffers_address,
                     sizeof(transfer_buffers_address)) &&
        kinetis_read(device, transfer_buffers_address + sizeof(force_frame), received,
                     sizeof(received));
    if (!readable || memcmp(received, force_frame, sizeof(received)) != 0) {
        fprintf(stderr, "received SPI frame:");
        for (size_t index = 0U; index < sizeof(received); ++index)
            fprintf(stderr, " %02x", received[index]);
        fprintf(stderr, "\n");
        return false;
    }
    return true;
}

static bool force_frame_checksum_matches(Kinetis *device) {
    const uint16_t checksum = (uint16_t)cortex_m4_get_register(kinetis_cpu(device), 1U);
    if (checksum != UINT16_C(0xe9fe))
        fprintf(stderr, "force frame checksum: 0x%04" PRIx16 "\n", checksum);
    return checksum == UINT16_C(0xe9fe);
}

static bool set_encoder_phase(Kinetis *device, bool phase_a, bool phase_b) {
    return kinetis_set_ftm_quadrature_input(device, 2U, 0U, phase_a) &&
           kinetis_set_ftm_quadrature_input(device, 2U, 1U, phase_b);
}

static bool stimulate_encoder(Kinetis *device, const char *path) {
    static const bool phases[][2] = {
        {true, false},
        {true, true},
        {false, true},
        {false, false},
    };
    for (size_t index = 0U; index < sizeof(phases) / sizeof(phases[0]); ++index) {
        if (!set_encoder_phase(device, phases[index][0], phases[index][1]) ||
            !run_phase(device, 10000U)) {
            return false;
        }
    }
    uint32_t pin_control = 0U;
    uint32_t interrupt_flags = 0U;
    const bool driven = kinetis_gpio_drive(device, 4U, 24U, false);
    const bool readable =
        kinetis_read(device, UINT32_C(0x4004d060), &pin_control, sizeof(pin_control)) &&
        kinetis_read(device, UINT32_C(0x4004d0a0), &interrupt_flags, sizeof(interrupt_flags));
    const bool pending = cortex_m4_get_irq_pending(kinetis_cpu(device), 31U);
    return driven && readable && (interrupt_flags & (1UL << 24U)) != 0U && pending &&
           run_to_symbol(device, path, "PORTB_PORTC_PORTD_PORTE_IRQHandler", 100000U) &&
           run_to_symbol(device, path, "motor_runtime_encoder_index_handler", 1000000U) &&
           run_phase(device, 1000U);
}

static bool wait_for_encoder_edge(Kinetis *device, bool increasing) {
    uint32_t before = 0U;
    uint32_t modulo = 0U;
    if (!kinetis_read(device, UINT32_C(0x4003a004), &before, sizeof(before)) ||
        !kinetis_read(device, UINT32_C(0x4003a008), &modulo, sizeof(modulo))) {
        return false;
    }
    const uint32_t expected = increasing     ? before == modulo ? 0U : before + 1U
                              : before == 0U ? modulo
                                             : before - 1U;
    for (uint32_t instruction = 0U; instruction < 100U; ++instruction) {
        uint32_t counter = 0U;
        if (cortex_m4_step(kinetis_cpu(device)).stop != CORTEX_M4_STOP_RUNNING ||
            !execution_valid(device) ||
            !kinetis_read(device, UINT32_C(0x4003a004), &counter, sizeof(counter))) {
            return false;
        }
        if (counter == expected) {
            return true;
        }
        if (counter != before) {
            return false;
        }
    }
    return false;
}

static bool stimulate_encoder_cycles(Kinetis *device, uint32_t cycles) {
    static const bool phases[][2] = {
        {true, false},
        {true, true},
        {false, true},
        {false, false},
    };
    for (uint32_t cycle = 0U; cycle < cycles; ++cycle) {
        for (size_t phase = 0U; phase < sizeof(phases) / sizeof(phases[0]); ++phase) {
            if (!set_encoder_phase(device, phases[phase][0], phases[phase][1]) ||
                !wait_for_encoder_edge(device, true)) {
                return false;
            }
        }
    }
    return true;
}

static bool stimulate_encoder_reverse_cycles(Kinetis *device, uint32_t cycles) {
    static const bool phases[][2] = {
        {false, true},
        {true, true},
        {true, false},
        {false, false},
    };
    for (uint32_t cycle = 0U; cycle < cycles; ++cycle) {
        for (size_t phase = 0U; phase < sizeof(phases) / sizeof(phases[0]); ++phase) {
            if (!set_encoder_phase(device, phases[phase][0], phases[phase][1]) ||
                !wait_for_encoder_edge(device, false)) {
                return false;
            }
        }
    }
    return true;
}

static bool encoder_revolution_cycles(Kinetis *device, uint32_t *cycles) {
    uint32_t modulus = 0U;
    if (!kinetis_read(device, UINT32_C(0x4003a008), &modulus, sizeof(modulus)) ||
        (modulus + 1U) % 4U != 0U) {
        return false;
    }
    *cycles = (modulus + 1U) / 4U;
    return true;
}

static bool stimulate_parameter_write(Kinetis *device, const char *path, const uint8_t *request,
                                      size_t size, bool split_start) {
    if (!kinetis_i2c_detect_start(device, KINETIS_SERIAL_I2C0) ||
        (split_start && (!run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) ||
                         !finish_interrupt(device, 1000U))) ||
        !kinetis_i2c_address(device, KINETIS_SERIAL_I2C0, 0x78U, false) ||
        !run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) ||
        !finish_interrupt(device, 1000U)) {
        return false;
    }
    for (size_t index = 0U; index < size; ++index) {
        if (!kinetis_i2c_receive(device, KINETIS_SERIAL_I2C0, request[index]) ||
            !run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) ||
            !finish_interrupt(device, 1000U)) {
            return false;
        }
    }
    if (!kinetis_i2c_detect_stop(device, KINETIS_SERIAL_I2C0))
        return false;
    if (!run_to_symbol(device, path, "motor_bus_transfer_callback", 100000U))
        return false;
    uint32_t state_symbol = 0U;
    uint8_t state = 0U;
    uint32_t event = 0U;
    uint32_t transfer = cortex_m4_get_register(kinetis_cpu(device), 1U);
    const bool completion = cortex_m4_elf_symbol(path, "motor_bus_state", &state_symbol) &&
                            kinetis_read(device, state_symbol, &state, sizeof(state)) &&
                            kinetis_read(device, transfer, &event, sizeof(event)) && state == 1U &&
                            event == UINT32_C(0x20);
    if (!completion) {
        fprintf(stderr, "parameter completion: state=%" PRIu8 " event=0x%02" PRIx32 "\n", state,
                event);
        return false;
    }
    if (!run_to_symbol(device, path, "motor_parameter_request_apply", 100000U))
        return false;
    const uint32_t received_size = cortex_m4_get_register(kinetis_cpu(device), 2U);
    if (received_size != size) {
        fprintf(stderr, "parameter request size: %" PRIu32 "\n", received_size);
        return false;
    }
    return run_phase(device, 30000U);
}

static bool parameter_read_matches(Kinetis *device, const char *path, uint8_t selector,
                                   const uint8_t *expected, size_t size);

static bool motor_bus_symbol_read(const Kinetis *device, const char *path, const char *name,
                                  void *value, size_t size) {
    uint32_t address = 0U;
    return cortex_m4_elf_symbol(path, name, &address) && kinetis_read(device, address, value, size);
}

static bool motor_bus_active_matches(const Kinetis *device, const char *path, bool expected) {
    uint8_t active = 0U;
    return motor_bus_symbol_read(device, path, "motor_bus_active", &active, sizeof(active)) &&
           active == expected;
}

static bool motor_bus_state_matches(const Kinetis *device, const char *path, uint8_t expected) {
    uint8_t state = 0U;
    return motor_bus_symbol_read(device, path, "motor_bus_state", &state, sizeof(state)) &&
           state == expected;
}

static bool motor_bus_interrupt(Kinetis *device, const char *path) {
    return run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) &&
           finish_interrupt(device, 1000U);
}

static bool motor_bus_service_tick(Kinetis *device, const char *path) {
    return run_to_symbol(device, path, "motor_runtime_service_handler", 1000000U) &&
           finish_interrupt(device, 1000000U);
}

static bool stimulate_bare_start_timeout(Kinetis *device, const char *path) {
    if (!kinetis_i2c_detect_start(device, KINETIS_SERIAL_I2C0) ||
        !motor_bus_interrupt(device, path) || !motor_bus_active_matches(device, path, true)) {
        return false;
    }
    for (uint8_t tick = 0U; tick < 9U; ++tick) {
        if (!motor_bus_service_tick(device, path) || !motor_bus_active_matches(device, path, true))
            return false;
    }
    if (!motor_bus_service_tick(device, path) || !motor_bus_active_matches(device, path, false) ||
        !motor_bus_state_matches(device, path, 0U))
        return false;
    return kinetis_i2c_detect_stop(device, KINETIS_SERIAL_I2C0);
}

static bool motor_bus_read_start(Kinetis *device, const char *path, uint8_t selector) {
    return kinetis_i2c_detect_start(device, KINETIS_SERIAL_I2C0) &&
           motor_bus_interrupt(device, path) &&
           kinetis_i2c_address(device, KINETIS_SERIAL_I2C0, 0x78U, false) &&
           motor_bus_interrupt(device, path) &&
           kinetis_i2c_receive(device, KINETIS_SERIAL_I2C0, selector) &&
           motor_bus_interrupt(device, path) &&
           kinetis_i2c_detect_start(device, KINETIS_SERIAL_I2C0) &&
           motor_bus_interrupt(device, path) &&
           kinetis_i2c_address(device, KINETIS_SERIAL_I2C0, 0x78U, true) &&
           motor_bus_interrupt(device, path);
}

static bool stimulate_partial_write_repeated_start(Kinetis *device, const char *path) {
    static const uint8_t partial[] = {32U, 0x11U};
    static const uint8_t complete[] = {32U, 0x82U};
    static const uint8_t expected[] = {0x82U, 0U, 0U, 0U, 1U};
    if (!kinetis_i2c_detect_start(device, KINETIS_SERIAL_I2C0) ||
        !motor_bus_interrupt(device, path) ||
        !kinetis_i2c_address(device, KINETIS_SERIAL_I2C0, 0x78U, false) ||
        !motor_bus_interrupt(device, path)) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(partial); ++index) {
        if (!kinetis_i2c_receive(device, KINETIS_SERIAL_I2C0, partial[index]) ||
            !motor_bus_interrupt(device, path)) {
            return false;
        }
    }
    if (!kinetis_i2c_detect_start(device, KINETIS_SERIAL_I2C0) ||
        !motor_bus_interrupt(device, path) ||
        !kinetis_i2c_address(device, KINETIS_SERIAL_I2C0, 0x78U, false) ||
        !motor_bus_interrupt(device, path)) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(complete); ++index) {
        if (!kinetis_i2c_receive(device, KINETIS_SERIAL_I2C0, complete[index]) ||
            !motor_bus_interrupt(device, path)) {
            return false;
        }
    }
    return kinetis_i2c_detect_stop(device, KINETIS_SERIAL_I2C0) &&
           motor_bus_interrupt(device, path) &&
           parameter_read_matches(device, path, 32U, expected, sizeof(expected));
}

static bool stimulate_early_read_nack(Kinetis *device, const char *path) {
    uint16_t value = 0U;
    if (!motor_bus_read_start(device, path, 32U) ||
        !kinetis_serial_transmit(device, KINETIS_SERIAL_I2C0, &value) ||
        !kinetis_i2c_acknowledge(device, KINETIS_SERIAL_I2C0, false) ||
        !motor_bus_interrupt(device, path) || !motor_bus_active_matches(device, path, true) ||
        !motor_bus_state_matches(device, path, 0U)) {
        return false;
    }
    for (uint8_t tick = 0U; tick < 9U; ++tick) {
        if (!motor_bus_service_tick(device, path) || !motor_bus_active_matches(device, path, true))
            return false;
    }
    if (!motor_bus_service_tick(device, path) || !motor_bus_active_matches(device, path, false))
        return false;
    return kinetis_i2c_detect_stop(device, KINETIS_SERIAL_I2C0);
}

static bool stimulate_final_read_nack(Kinetis *device, const char *path) {
    static const uint8_t expected[] = {0x82U, 0U, 0U, 0U, 1U};
    uint16_t value = 0U;
    if (!motor_bus_read_start(device, path, 32U))
        return false;
    for (size_t index = 0U; index < sizeof(expected); ++index) {
        if (!kinetis_serial_transmit(device, KINETIS_SERIAL_I2C0, &value) ||
            value != expected[index] ||
            !kinetis_i2c_acknowledge(device, KINETIS_SERIAL_I2C0, index + 1U < sizeof(expected)) ||
            !motor_bus_interrupt(device, path)) {
            return false;
        }
    }
    return motor_bus_active_matches(device, path, true) &&
           kinetis_i2c_detect_stop(device, KINETIS_SERIAL_I2C0) &&
           motor_bus_interrupt(device, path) && motor_bus_active_matches(device, path, false) &&
           motor_bus_state_matches(device, path, 0U);
}

static bool stimulate_read_overread(Kinetis *device, const char *path) {
    static const uint8_t expected[] = {0x82U, 0U, 0U, 0U, 1U};
    uint16_t value = 0U;
    if (!motor_bus_read_start(device, path, 32U))
        return false;
    for (size_t index = 0U; index < sizeof(expected); ++index) {
        if (!kinetis_serial_transmit(device, KINETIS_SERIAL_I2C0, &value) ||
            value != expected[index] ||
            !kinetis_i2c_acknowledge(device, KINETIS_SERIAL_I2C0, true) ||
            !motor_bus_interrupt(device, path)) {
            return false;
        }
    }
    if (kinetis_serial_transmit(device, KINETIS_SERIAL_I2C0, &value) ||
        !motor_bus_active_matches(device, path, true) ||
        !motor_bus_state_matches(device, path, 0U)) {
        return false;
    }
    return kinetis_i2c_detect_stop(device, KINETIS_SERIAL_I2C0) &&
           motor_bus_interrupt(device, path) && motor_bus_active_matches(device, path, false);
}

static bool parameter_read_matches(Kinetis *device, const char *path, uint8_t selector,
                                   const uint8_t *expected, size_t size) {
    if (!kinetis_i2c_detect_start(device, KINETIS_SERIAL_I2C0) ||
        !run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) ||
        !finish_interrupt(device, 1000U) ||
        !kinetis_i2c_address(device, KINETIS_SERIAL_I2C0, 0x78U, false) ||
        !run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) ||
        !finish_interrupt(device, 1000U) ||
        !kinetis_i2c_receive(device, KINETIS_SERIAL_I2C0, selector) ||
        !run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) ||
        !finish_interrupt(device, 1000U) ||
        !kinetis_i2c_detect_start(device, KINETIS_SERIAL_I2C0) ||
        !run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) ||
        !finish_interrupt(device, 1000U) ||
        !kinetis_i2c_address(device, KINETIS_SERIAL_I2C0, 0x78U, true) ||
        !run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) ||
        !finish_interrupt(device, 1000U)) {
        return false;
    }

    for (size_t index = 0U; index < size; ++index) {
        uint16_t value = 0U;
        if (!kinetis_serial_transmit(device, KINETIS_SERIAL_I2C0, &value) ||
            value != expected[index] ||
            !kinetis_i2c_acknowledge(device, KINETIS_SERIAL_I2C0, index + 1U < size) ||
            !run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) ||
            !finish_interrupt(device, 1000U)) {
            fprintf(stderr, "parameter response byte %zu: 0x%02" PRIx16 "\n", index, value);
            return false;
        }
    }
    return kinetis_i2c_detect_stop(device, KINETIS_SERIAL_I2C0) &&
           run_to_symbol(device, path, "I2C0_IRQHandler", 100000U) &&
           finish_interrupt(device, 1000U);
}

static bool trigger_encoder_index_handler(Kinetis *device, const char *path) {
    uint32_t interrupt_flags = 0U;
    return kinetis_gpio_drive(device, 4U, 24U, false) &&
           kinetis_gpio_drive(device, 4U, 24U, true) &&
           kinetis_gpio_drive(device, 4U, 24U, false) &&
           kinetis_read(device, UINT32_C(0x4004d0a0), &interrupt_flags, sizeof(interrupt_flags)) &&
           (interrupt_flags & (1UL << 24U)) != 0U &&
           cortex_m4_get_irq_pending(kinetis_cpu(device), 31U) &&
           run_to_symbol(device, path, "PORTB_PORTC_PORTD_PORTE_IRQHandler", 100000U) &&
           run_to_symbol(device, path, "motor_runtime_encoder_index_handler", 1000000U);
}

static bool trigger_encoder_index(Kinetis *device, const char *path) {
    return trigger_encoder_index_handler(device, path) && run_phase(device, 1000U);
}

static bool direction_revolution_matches(Kinetis *device) {
    const uint32_t state = cortex_m4_get_register(kinetis_cpu(device), 1U);
    const bool complete = cortex_m4_get_register(kinetis_cpu(device), 2U) != 0U;
    const int32_t position = (int32_t)cortex_m4_get_register(kinetis_cpu(device), 3U);
    const uint32_t stack = cortex_m4_get_register(kinetis_cpu(device), 13U);
    int32_t first_index = 0;
    int32_t modulus = 0;
    const bool readable = kinetis_read(device, state + 8U, &first_index, sizeof(first_index)) &&
                          kinetis_read(device, stack, &modulus, sizeof(modulus));
    const int32_t error = position - modulus - first_index;
    if (!readable || !complete || error <= -10 || error >= 10)
        fprintf(stderr,
                "direction revolution: complete=%u position=%" PRId32 " first=%" PRId32
                " modulus=%" PRId32 "\n",
                complete, position, first_index, modulus);
    return readable && complete && error > -10 && error < 10;
}

static bool stimulate_encoder_direction_failure(Kinetis *device, const char *path) {
    if (!stimulate_parameter_write(device, path, encoder_direction_request,
                                   sizeof(encoder_direction_request), false) ||
        !run_to_symbol(device, path, "motor_encoder_direction_check_step", 10000000U) ||
        !run_until_index_interrupt_armed(device) || !trigger_encoder_index(device, path) ||
        !run_until_index_interrupt_armed(device) || !trigger_encoder_index(device, path)) {
        return false;
    }
    return run_phase(device, 100000U);
}

static bool stimulate_encoder_direction_pass(Kinetis *device, const char *path) {
    if (!stimulate_parameter_write(device, path, encoder_direction_request,
                                   sizeof(encoder_direction_request), true) ||
        !run_to_symbol(device, path, "motor_encoder_direction_check_step", 10000000U) ||
        !run_until_index_interrupt_armed(device) || !trigger_encoder_index(device, path) ||
        !run_until_index_interrupt_armed(device)) {
        return false;
    }
    uint32_t cycles = 0U;
    if (!encoder_revolution_cycles(device, &cycles) || !stimulate_encoder_cycles(device, cycles) ||
        !trigger_encoder_index_handler(device, path) ||
        !run_to_symbol(device, path, "motor_encoder_direction_check_step", 10000000U) ||
        !direction_revolution_matches(device) || !run_phase(device, 1000U) ||
        !stimulate_encoder_reverse_cycles(device, cycles) || !run_phase(device, 100000U)) {
        return false;
    }
    return parameter_read_matches(device, path, 5U, encoder_direction_complete_response,
                                  sizeof(encoder_direction_complete_response));
}

static bool executed_symbol(const char *path, const CortexM4Coverage *coverage, const char *name) {
    uint32_t address = 0U;
    const bool found = cortex_m4_elf_symbol(path, name, &address);
    address &= ~1U;
    const bool executed =
        found && (cortex_m4_coverage_flags(coverage, address) & CORTEX_M4_COVERAGE_EXECUTED) != 0U;
    if (!executed)
        fprintf(stderr, "unreached firmware checkpoint: %s\n", name);
    return executed;
}

static bool i2c_configuration_matches(Kinetis *device) {
    uint32_t filter = 0U;
    uint32_t secondary_control = 0U;
    return kinetis_read(device, UINT32_C(0x40066006), &filter, sizeof(uint8_t)) &&
           kinetis_read(device, UINT32_C(0x40066005), &secondary_control, sizeof(uint8_t)) &&
           filter == 0x24U && (secondary_control & UINT32_C(0x40)) == 0U;
}

static size_t select_owned_coverage(CortexM4Coverage *coverage, const char *path) {
    static const char *prefixes[] = {
        "motor_",
        "firmware_main",
        "SystemInitHook",
        "ADC0_IRQHandler",
        "ADC1_IRQHandler",
        "DMA0_DMA4_IRQHandler",
        "DMA1_DMA5_IRQHandler",
        "FTM2_IRQHandler",
        "FTM3_IRQHandler",
        "FTM4_IRQHandler",
        "I2C0_IRQHandler",
        "PDB0_PDB1_IRQHandler",
        "PORTB_PORTC_PORTD_PORTE_IRQHandler",
        "WDOG_EWM_IRQHandler",
    };
    size_t selected = 0U;
    for (size_t index = 0U; index < sizeof(prefixes) / sizeof(prefixes[0]); ++index)
        selected += cortex_m4_coverage_select_elf_functions(coverage, path, prefixes[index]);
    return selected;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return EXIT_FAILURE;

    KinetisConfiguration configuration = kinetis_configuration(KINETIS_PROFILE_MKV10Z1287);
    configuration.package = KINETIS_PACKAGE_LH_64_LQFP;
    configuration.vector_table_address = 0xa000U;
    Kinetis *device = kinetis_create(configuration);
    if (device == NULL)
        return EXIT_FAILURE;

    uint32_t entry_address = 0U;
    CortexM4Coverage *coverage = cortex_m4_coverage_create_elf(argv[1]);
    const size_t owned_functions = select_owned_coverage(coverage, argv[1]);
    bool passed = coverage != NULL && owned_functions != 0U;
    if (passed && !cortex_m4_load_elf(device, argv[1], &entry_address)) {
        fprintf(stderr, "startup preparation failed: load\n");
        passed = false;
    }
    if (passed && !kinetis_reset(device)) {
        fprintf(stderr, "startup preparation failed: reset\n");
        passed = false;
    }
    if (passed && !configure_inputs(device)) {
        fprintf(stderr, "startup preparation failed: inputs\n");
        passed = false;
    }
    if (passed && !queue_frame(device, force_frame, sizeof(force_frame))) {
        fprintf(stderr, "startup preparation failed: frame\n");
        passed = false;
    }
    if (passed && !queue_effect_frames(device)) {
        fprintf(stderr, "startup preparation failed: effects\n");
        passed = false;
    }
    if (passed) {
        cortex_m4_set_coverage(kinetis_cpu(device), coverage);
        passed = run_to_symbol(device, argv[1], "motor_link_frame_decode_checked", 10000000U) &&
                 adc_calibration_configuration_matches(device) &&
                 startup_output_state_safe(device) && force_frame_checksum_matches(device) &&
                 received_force_frame(argv[1], device) &&
                 run_to_symbol(device, argv[1], "motor_protocol_frame_apply", 10000000U) &&
                 run_to_symbol(device, argv[1], "motor_runtime_poll", 20000000U) &&
                 pdb_status_clear_matches(device, argv[1]) && i2c_configuration_matches(device);
        const uint64_t initialization_reads = kinetis_get_uninitialized_sram_read_count(device);
        if (initialization_reads != 0U)
            fprintf(stderr,
                    "unexpected initialization SRAM reads: count=%" PRIu64 " first=0x%08" PRIx32
                    "\n",
                    initialization_reads, kinetis_get_first_uninitialized_sram_read(device));
        passed = passed && initialization_reads == 0U;
        kinetis_clear_uninitialized_sram_reads(device);
        static const uint8_t steering_range_response[] = {0x82U, 0U, 0U, 0U, 1U};
        passed = passed && stimulate_bare_start_timeout(device, argv[1]) &&
                 stimulate_partial_write_repeated_start(device, argv[1]) &&
                 stimulate_early_read_nack(device, argv[1]) &&
                 stimulate_final_read_nack(device, argv[1]) &&
                 stimulate_read_overread(device, argv[1]) &&
                 stimulate_parameter_write(device, argv[1], steering_range_request,
                                           sizeof(steering_range_request), true) &&
                 parameter_read_matches(device, argv[1], 32U, steering_range_response,
                                        sizeof(steering_range_response)) &&
                 run_phase(device, 10000U) &&
                 run_to_symbol(device, argv[1], "motor_spi_link_active_set", 100000000U) &&
                 apply_effect_frames(device, argv[1]) && run_until_index_interrupt_armed(device) &&
                 run_until_encoder_running(device) && stimulate_encoder_cycles(device, 100U) &&
                 stimulate_encoder(device, argv[1]) &&
                 stimulate_encoder_direction_failure(device, argv[1]) &&
                 stimulate_encoder_direction_pass(device, argv[1]) && run_phase(device, 1000000U) &&
                 ftm2_reinitialization_matches(device, argv[1]) &&
                 fatal_output_state_safe(device, argv[1]);
    }

    size_t spi_transfers = 0U;
    KinetisSpiTransfer spi_transfer;
    while (kinetis_spi_transfer(device, KINETIS_SERIAL_SPI0, &spi_transfer))
        ++spi_transfers;

    CortexM4CoverageResult result = {0};
    CortexM4CoverageResult owned_result = {0};
    if (coverage != NULL)
        result = cortex_m4_coverage_result(coverage);
    if (coverage != NULL)
        owned_result = cortex_m4_coverage_selected_result(coverage);
    const uint64_t uninitialized_reads = kinetis_get_uninitialized_sram_read_count(device);
    if (uninitialized_reads != 0U)
        fprintf(stderr, "uninitialized SRAM reads: count=%" PRIu64 " first=0x%08" PRIx32 "\n",
                uninitialized_reads, kinetis_get_first_uninitialized_sram_read(device));
    printf("entry=0x%08" PRIx32 " cfsr=0x%08" PRIx32
           " spi=%zu coverage=%zu/%zu branch-sites=%zu/%zu "
           "owned-functions=%zu owned-coverage=%zu/%zu (%.2f%%) "
           "owned-branches=%zu/%zu (%.2f%%)\n",
           entry_address, cortex_m4_get_fault_status(kinetis_cpu(device)), spi_transfers,
           result.covered_instructions, result.total_instructions, result.covered_branch_sites,
           result.total_branch_sites, owned_functions, owned_result.covered_instructions,
           owned_result.total_instructions, owned_result.instruction_coverage_percent,
           owned_result.covered_branch_outcomes, owned_result.total_branch_outcomes,
           owned_result.branch_outcome_coverage_percent);

    static const char *checkpoints[] = {
        "firmware_main",
        "motor_runtime_initialize",
        "motor_runtime_poll",
        "motor_runtime_service_handler",
        "motor_runtime_adc_handler",
        "motor_runtime_control_cycle",
        "motor_foc_step",
        "motor_runtime_spi_receive",
        "motor_bus_transfer_callback",
        "motor_protocol_frame_apply",
        "motor_force_feedback_command_apply",
        "motor_force_feedback_mix",
        "motor_force_feedback_soft_stop_apply",
        "motor_parameter_request_apply",
        "motor_runtime_encoder_index_handler",
        "motor_encoder_direction_check_step",
        "motor_velocity_control_controller_reset",
    };
    passed = passed && spi_transfers >= sizeof(force_frame) + sizeof(effect_frames) &&
             uninitialized_reads == 0U;
    for (size_t index = 0U; index < sizeof(checkpoints) / sizeof(checkpoints[0]); ++index)
        passed = executed_symbol(argv[1], coverage, checkpoints[index]) && passed;
    cortex_m4_set_coverage(kinetis_cpu(device), NULL);
    cortex_m4_coverage_destroy(coverage);
    kinetis_destroy(device);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
