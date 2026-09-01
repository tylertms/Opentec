int motor_test_storage(void);

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

_Noreturn void motor_storage_test_main(void) {
    if (motor_test_storage() != 0)
        __asm volatile("udf #0");
    for (;;) {
    }
}
