#include <assert.h>

#include "common/motor/profile.h"

int main(void) {
    MotorHardwareProfile standard = motor_hardware_profile_select(false);
    assert(standard.encoder_period == 0x5c80U);
    assert(standard.encoder_modulus == 0x5c7fU);
    assert(standard.position_limit == 0x00028780U);
    assert(standard.position_scale == 0x000f3851U);
    assert(standard.velocity_scale == 0x002985a1U);
    assert(standard.secondary_scale == 0x0020e374U);
    assert(standard.correction_table_length == 0x0940U);
    assert(standard.adc_auxiliary_channel == 7U);

    MotorHardwareProfile alternate = motor_hardware_profile_select(true);
    assert(alternate.encoder_period == 0x5d2cU);
    assert(alternate.encoder_modulus == 0x5d2bU);
    assert(alternate.position_limit == 0x00028c34U);
    assert(alternate.position_scale == 0x000f1ca2U);
    assert(alternate.velocity_scale == 0x0029374bU);
    assert(alternate.secondary_scale == 0x002120a3U);
    assert(alternate.correction_table_length == 0x0951U);
    assert(alternate.adc_auxiliary_channel == 4U);

    return 0;
}
