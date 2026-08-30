#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cortex_m4_firmware_image.h"

static const uint8_t force_frame[] = {
    0x7bU, 0x01U, 0x05U, 0x06U, 0x07U, 0x08U, 0xcdU, 0xabU, 0x68U, 0x24U, 0xfeU, 0xe9U, 0x7dU,
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
           kinetis_set_ftm_input(device, 2U, 0U, false) &&
           kinetis_set_ftm_input(device, 2U, 1U, false);
}

static bool queue_force_frame(Kinetis *device) {
    for (size_t index = 0U; index < sizeof(force_frame); ++index) {
        if (!kinetis_serial_receive(device, KINETIS_SERIAL_SPI0, force_frame[index], 0U))
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

static bool stimulate_encoder(Kinetis *device, const char *path) {
    static const bool phases[][2] = {
        {true, false},
        {true, true},
        {false, true},
        {false, false},
    };
    for (size_t index = 0U; index < sizeof(phases) / sizeof(phases[0]); ++index) {
        if (!kinetis_set_ftm_input(device, 2U, 0U, phases[index][0]) ||
            !kinetis_set_ftm_input(device, 2U, 1U, phases[index][1]) ||
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

static bool stimulate_parameter_write(Kinetis *device) {
    static const uint8_t request[] = {32U, 0x55U};
    if (!kinetis_i2c_detect_start(device, KINETIS_SERIAL_I2C0) ||
        !kinetis_i2c_address(device, KINETIS_SERIAL_I2C0, 0x78U, false) ||
        !run_phase(device, 20000U)) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(request); ++index) {
        if (!kinetis_i2c_receive(device, KINETIS_SERIAL_I2C0, request[index]) ||
            !run_phase(device, 20000U)) {
            return false;
        }
    }
    return kinetis_i2c_detect_stop(device, KINETIS_SERIAL_I2C0) && run_phase(device, 30000U);
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
    bool passed = coverage != NULL && cortex_m4_load_elf(device, argv[1], &entry_address) &&
                  kinetis_reset(device) && configure_inputs(device) && queue_force_frame(device);
    if (passed) {
        cortex_m4_set_coverage(kinetis_cpu(device), coverage);
        passed = run_to_symbol(device, argv[1], "motor_link_frame_decode_checked", 10000000U) &&
                 force_frame_checksum_matches(device) && received_force_frame(argv[1], device) &&
                 run_to_symbol(device, argv[1], "motor_protocol_frame_apply", 10000000U) &&
                 run_to_symbol(device, argv[1], "motor_runtime_poll", 20000000U);
        const uint64_t initialization_reads = kinetis_get_uninitialized_sram_read_count(device);
        if (initialization_reads != 2U)
            fprintf(stderr,
                    "unexpected initialization SRAM reads: count=%" PRIu64 " first=0x%08" PRIx32
                    "\n",
                    initialization_reads, kinetis_get_first_uninitialized_sram_read(device));
        passed = passed && initialization_reads == 2U;
        kinetis_clear_uninitialized_sram_reads(device);
        passed = passed && run_phase(device, 10000U) && run_until_index_interrupt_armed(device) &&
                 stimulate_encoder(device, argv[1]) && stimulate_parameter_write(device) &&
                 run_phase(device, 1000000U);
    }

    size_t spi_transfers = 0U;
    KinetisSpiTransfer spi_transfer;
    while (kinetis_spi_transfer(device, KINETIS_SERIAL_SPI0, &spi_transfer))
        ++spi_transfers;

    CortexM4CoverageResult result = {0};
    if (coverage != NULL)
        result = cortex_m4_coverage_result(coverage);
    const uint64_t uninitialized_reads = kinetis_get_uninitialized_sram_read_count(device);
    if (uninitialized_reads != 0U)
        fprintf(stderr, "uninitialized SRAM reads: count=%" PRIu64 " first=0x%08" PRIx32 "\n",
                uninitialized_reads, kinetis_get_first_uninitialized_sram_read(device));
    printf("entry=0x%08" PRIx32 " cfsr=0x%08" PRIx32
           " spi=%zu coverage=%zu/%zu branch-sites=%zu/%zu\n",
           entry_address, cortex_m4_get_fault_status(kinetis_cpu(device)), spi_transfers,
           result.covered_instructions, result.total_instructions, result.covered_branch_sites,
           result.total_branch_sites);

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
        "motor_parameter_request_apply",
        "motor_runtime_encoder_index_handler",
    };
    passed = passed && spi_transfers >= sizeof(force_frame) && uninitialized_reads == 0U;
    for (size_t index = 0U; index < sizeof(checkpoints) / sizeof(checkpoints[0]); ++index)
        passed = executed_symbol(argv[1], coverage, checkpoints[index]) && passed;
    cortex_m4_set_coverage(kinetis_cpu(device), NULL);
    cortex_m4_coverage_destroy(coverage);
    kinetis_destroy(device);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
