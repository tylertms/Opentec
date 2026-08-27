project('mcux-sdk', 'c')

mcux_sdk_includes = include_directories(
  'devices/MKV30F12810',
  'devices/MKV30F12810/drivers',
  'drivers/common',
  'drivers/adc16',
  'drivers/dmamux',
  'drivers/dspi',
  'drivers/edma',
  'drivers/gpio',
  'drivers/i2c',
  'drivers/lptmr',
  'drivers/pit',
  'drivers/port',
  'drivers/smc',
  'drivers/uart',
  'drivers/wdog',
  is_system: true,
)
mcux_sdk_sources = {
  'clock': files('devices/MKV30F12810/drivers/fsl_clock.c'),
  'common': files('drivers/common/fsl_common.c'),
  'adc16': files('drivers/adc16/fsl_adc16.c'),
  'dmamux': files('drivers/dmamux/fsl_dmamux.c'),
  'dspi': files('drivers/dspi/fsl_dspi.c'),
  'edma': files('drivers/edma/fsl_edma.c'),
  'gpio': files('drivers/gpio/fsl_gpio.c'),
  'i2c': files('drivers/i2c/fsl_i2c.c'),
  'lptmr': files('drivers/lptmr/fsl_lptmr.c'),
  'pit': files('drivers/pit/fsl_pit.c'),
  'smc': files('drivers/smc/fsl_smc.c'),
  'uart': files('drivers/uart/fsl_uart.c'),
  'uart_edma': files('drivers/uart/fsl_uart_edma.c'),
  'wdog': files('drivers/wdog/fsl_wdog.c'),
}
mcux_sdk_device_sources = files(
  'devices/MKV30F12810/system_MKV30F12810.c',
  'devices/MKV30F12810/gcc/startup_MKV30F12810.S',
)
