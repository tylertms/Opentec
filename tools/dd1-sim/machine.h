#ifndef OPENTEC_DD1_SIM_MACHINE_H
#define OPENTEC_DD1_SIM_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    DD1_SIM_DISPLAY_WIDTH = 256,
    DD1_SIM_DISPLAY_HEIGHT = 64,
    DD1_SIM_DISPLAY_SIZE = 8192,
    DD1_SIM_STATUS_SIZE = 160,
};

typedef struct Dd1SimMachine Dd1SimMachine;

typedef struct {
    uint8_t display[DD1_SIM_DISPLAY_SIZE];
    uint8_t wheel_output[33];
    uint64_t base_instructions;
    uint64_t wqr_instructions;
    uint64_t motor_instructions;
    uint32_t base_program_counter;
    uint32_t wqr_program_counter;
    uint32_t motor_program_counter;
    uint32_t base_clock_hz;
    uint32_t wqr_clock_hz;
    uint32_t motor_clock_hz;
    uint64_t display_bytes;
    uint64_t base_uart_bytes;
    uint64_t wqr_uart_bytes;
    uint64_t base_motor_words;
    uint64_t motor_base_words;
    uint32_t wheel_exchanges;
    uint32_t buttons;
    int32_t motor_position;
    int16_t motor_current;
    int16_t angle_tenths;
    bool display_ready;
    bool running;
    char status[DD1_SIM_STATUS_SIZE];
} Dd1SimSnapshot;

Dd1SimMachine *dd1_sim_machine_create(const char *base_path, const char *wqr_path,
                                      const char *motor_path, char *error, size_t error_size);
void dd1_sim_machine_destroy(Dd1SimMachine *machine);
bool dd1_sim_machine_run(Dd1SimMachine *machine, uint32_t instruction_rounds);
void dd1_sim_machine_set_parallel(Dd1SimMachine *machine, bool parallel);
void dd1_sim_machine_set_inputs(Dd1SimMachine *machine, int16_t angle_tenths, uint32_t buttons);
void dd1_sim_machine_snapshot(const Dd1SimMachine *machine, Dd1SimSnapshot *snapshot);

#endif
