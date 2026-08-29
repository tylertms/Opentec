#include "common/motor/communication.h"

#include <fsl_i2c.h>

/**
 * @brief Configures the motor I2C peripheral for extended address 0x78 and general calls.
 */
void motor_bus_initialize(void) {
    i2c_slave_config_t config;

    I2C_SlaveGetDefaultConfig(&config);
    config.enableGeneralCall = true;
    config.slaveAddress = 0x78U;
    I2C_SlaveInit(I2C0, &config, CLOCK_GetFreq(kCLOCK_BusClk));

    I2C0->C2 |= I2C_C2_ADEXT_MASK;
    I2C0->C1 = (I2C0->C1 & (I2C_C1_IICEN_MASK | I2C_C1_MST_MASK)) | I2C_C1_RSTA_MASK;
    I2C0->F = 0x27U;
    I2C0->FLT = 0U;
    I2C0->SMB = 0xc2U;
}
