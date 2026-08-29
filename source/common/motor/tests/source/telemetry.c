#include "common/motor/telemetry.h"

#include <assert.h>

static void test_temperature_interpolation(void) {
    assert(motor_temperature_interpolate(3987U, kMotorTemperaturePrimary) == -255);
    assert(motor_temperature_interpolate(3970U, kMotorTemperaturePrimary) == -13);
    assert(motor_temperature_interpolate(3953U, kMotorTemperaturePrimary) == -11);
    assert(motor_temperature_interpolate(428U, kMotorTemperaturePrimary) == 255);

    assert(motor_temperature_interpolate(3948U, kMotorTemperatureSecondary) == -255);
    assert(motor_temperature_interpolate(3930U, kMotorTemperatureSecondary) == -13);
    assert(motor_temperature_interpolate(718U, kMotorTemperatureSecondary) == 255);
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
    assert(telemetry.primary_average == 3970U);
    assert(telemetry.secondary_average == 3930U);
    assert(telemetry.primary_temperature == -13);
    assert(telemetry.secondary_temperature == -13);
    assert(!motor_auxiliary_samples_resolve(&accumulator, &telemetry));
}

int main(void) {
    test_temperature_interpolation();
    test_auxiliary_accumulation();
    return 0;
}
