#include <assert.h>

#include "telemetry/auxiliary.h"

static void test_temperature_interpolation(void) {
    assert(motor_temperature_interpolate(3987U, kMotorTemperatureMotor) == -255);
    assert(motor_temperature_interpolate(3970U, kMotorTemperatureMotor) == -13);
    assert(motor_temperature_interpolate(3953U, kMotorTemperatureMotor) == -11);
    assert(motor_temperature_interpolate(428U, kMotorTemperatureMotor) == 255);

    assert(motor_temperature_interpolate(3948U, kMotorTemperatureDriver) == -255);
    assert(motor_temperature_interpolate(3930U, kMotorTemperatureDriver) == -13);
    assert(motor_temperature_interpolate(718U, kMotorTemperatureDriver) == 255);
}

static void test_auxiliary_accumulation(void) {
    MotorAuxiliaryAccumulator accumulator = {0};
    MotorAuxiliaryTelemetry telemetry;

    for (uint32_t index = 0U; index < 9999U; ++index) {
        assert(!motor_auxiliary_samples_accumulate(&accumulator, 3970U, 3930U));
    }
    assert(motor_auxiliary_samples_accumulate(&accumulator, 3970U, 3930U));
    assert(accumulator.count == 0U);
    assert(accumulator.ready);
    assert(motor_auxiliary_samples_resolve(&accumulator, &telemetry));
    assert(telemetry.motor_average == 3970U);
    assert(telemetry.driver_average == 3930U);
    assert(telemetry.motor_temperature == -13);
    assert(telemetry.driver_temperature == -13);
    assert(!motor_auxiliary_samples_resolve(&accumulator, &telemetry));
}

int main(void) {
    test_temperature_interpolation();
    test_auxiliary_accumulation();
    return 0;
}
