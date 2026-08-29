project('mcux-sdk', 'c')

mcux_sdk_driver_includes = include_directories(
  'drivers/common',
  'drivers/adc16',
  'drivers/dac',
  'drivers/dmamux',
  'drivers/dspi',
  'drivers/edma',
  'drivers/ftm',
  'drivers/gpio',
  'drivers/i2c',
  'drivers/lptmr',
  'drivers/pdb',
  'drivers/pit',
  'drivers/port',
  'drivers/smc',
  'drivers/uart',
  'drivers/wdog',
  is_system: true,
)

mcux_sdk_driver_sources = {
  'common': files('drivers/common/fsl_common.c'),
  'adc16': files('drivers/adc16/fsl_adc16.c'),
  'dac': files('drivers/dac/fsl_dac.c'),
  'dmamux': files('drivers/dmamux/fsl_dmamux.c'),
  'dspi': files('drivers/dspi/fsl_dspi.c'),
  'edma': files('drivers/edma/fsl_edma.c'),
  'ftm': files('drivers/ftm/fsl_ftm.c'),
  'gpio': files('drivers/gpio/fsl_gpio.c'),
  'i2c': files('drivers/i2c/fsl_i2c.c'),
  'lptmr': files('drivers/lptmr/fsl_lptmr.c'),
  'pdb': files('drivers/pdb/fsl_pdb.c'),
  'pit': files('drivers/pit/fsl_pit.c'),
  'smc': files('drivers/smc/fsl_smc.c'),
  'uart': files('drivers/uart/fsl_uart.c'),
  'uart_edma': files('drivers/uart/fsl_uart_edma.c'),
  'wdog': files('drivers/wdog/fsl_wdog.c'),
}

mcux_sdk_devices = {
  'MKV10Z1287': {
    'clock': files('devices/MKV10Z1287/drivers/fsl_clock.c'),
    'includes': include_directories(
      'devices/MKV10Z1287',
      'devices/MKV10Z1287/drivers',
      is_system: true,
    ),
    'sources': files(
      'devices/MKV10Z1287/system_MKV10Z1287.c',
      'devices/MKV10Z1287/gcc/startup_MKV10Z1287.S',
    ),
  },
  'MKV30F12810': {
    'clock': files('devices/MKV30F12810/drivers/fsl_clock.c'),
    'includes': include_directories(
      'devices/MKV30F12810',
      'devices/MKV30F12810/drivers',
      is_system: true,
    ),
    'sources': files(
      'devices/MKV30F12810/system_MKV30F12810.c',
      'devices/MKV30F12810/gcc/startup_MKV30F12810.S',
    ),
  },
}
