#include <stddef.h>

int motor_test_center(void);
int motor_test_control(void);
int motor_test_current_calibration(void);
int motor_test_drive(void);
int motor_test_encoder(void);
int motor_test_encoder_calibration(void);
int motor_test_foc(void);
int motor_test_force_feedback(void);
int motor_test_hardware_profile(void);
int motor_test_link_frame(void);
int motor_test_motion(void);
int motor_test_parameter(void);
int motor_test_pi(void);
int motor_test_protocol(void);
int motor_test_service_timing(void);
int motor_test_telemetry(void);
int motor_test_velocity_control(void);

_Noreturn void __assert_func(const char *file, int line, const char *function,
                             const char *expression) {
    (void)file;
    (void)line;
    (void)function;
    (void)expression;
    __asm volatile("udf #0");
    for (;;) {
    }
}

_Noreturn void motor_test_main(void) {
    int result = motor_test_current_calibration() | motor_test_control() | motor_test_pi() |
                 motor_test_foc() | motor_test_velocity_control() |
                 motor_test_center() | motor_test_encoder() | motor_test_encoder_calibration() |
                 motor_test_service_timing() | motor_test_force_feedback() |
                 motor_test_link_frame() | motor_test_drive() | motor_test_hardware_profile() |
                 motor_test_motion() | motor_test_telemetry() | motor_test_parameter() |
                 motor_test_protocol();
    if (result != 0)
        __asm volatile("udf #0");
    for (;;) {
    }
}
