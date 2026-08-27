protocol_sources = files('source/protocol.c')
protocol_include = include_directories('source')

if meson.is_cross_build()
  mcux_sdk = subproject('mcux-sdk')
  cmsis = subproject('cmsis')
  sdk_include = mcux_sdk.get_variable('mcux_sdk_includes')
  cmsis_include = cmsis.get_variable('cmsis_includes')
  sdk_sources = mcux_sdk.get_variable('mcux_sdk_sources')
  sdk_device_sources = mcux_sdk.get_variable('mcux_sdk_device_sources')
  linker_script = files('source/firmware.ld')[0]
  sdk_drivers = static_library(
    'podium-dd1-dd2-wqr-sdk',
    sdk_sources['clock'],
    sdk_sources['common'],
    sdk_sources['adc16'],
    sdk_sources['dmamux'],
    sdk_sources['dspi'],
    sdk_sources['edma'],
    sdk_sources['gpio'],
    sdk_sources['i2c'],
    sdk_sources['lptmr'],
    sdk_sources['pit'],
    sdk_sources['smc'],
    sdk_sources['uart'],
    sdk_sources['uart_edma'],
    sdk_sources['wdog'],
    build_by_default: false,
    c_args: [
      '-ffreestanding',
      '-ffunction-sections',
      '-fdata-sections',
      '-fno-pic',
      '-fno-pie',
      '-DCPU_MKV30F128VLH10',
      '-DNDEBUG',
    ],
    include_directories: [sdk_include, cmsis_include],
    override_options: ['warning_level=0'],
    pic: false,
  )
  objcopy = find_program('objcopy')
  python = import('python').find_installation()

  firmware = executable(
    'podium-dd1-dd2-wqr',
    protocol_sources,
    'source/platform/kinetis/platform.c',
    sdk_device_sources,
    build_by_default: false,
    c_args: [
      '-ffreestanding',
      '-ffunction-sections',
      '-fdata-sections',
      '-fno-common',
      '-fno-pic',
      '-fno-pie',
      '-DCPU_MKV30F128VLH10',
      '-DNDEBUG',
      '-D__START=firmware_main',
      '-D__STARTUP_CLEAR_BSS',
    ],
    include_directories: [protocol_include, sdk_include, cmsis_include],
    link_args: [
      '-nostartfiles',
      '-no-pie',
      '-Wl,--gc-sections',
      '-T' + linker_script.full_path(),
    ],
    link_depends: linker_script,
    link_with: sdk_drivers,
    name_suffix: 'elf',
  )
  unchecksummed_firmware = custom_target(
    'podium-dd1-dd2-wqr-binary',
    input: firmware,
    output: 'podium-dd1-dd2-wqr.unchecksummed.bin',
    command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
    build_subdir: 'podium-dd1-dd2-wqr.elf.p',
  )
  firmware_image = custom_target(
    'podium-dd1-dd2-wqr-image',
    input: unchecksummed_firmware,
    output: 'podium-dd1-dd2-wqr.bin',
    command: [
      python,
      meson.project_source_root() / 'tools/checksum.py',
      '@INPUT@',
      '@OUTPUT@',
    ],
  )
  firmware_hex = custom_target(
    'podium-dd1-dd2-wqr-intel-hex',
    input: firmware_image,
    output: 'podium-dd1-dd2-wqr.unencrypted.hex',
    command: [
      objcopy,
      '-I',
      'binary',
      '-O',
      'ihex',
      '--change-addresses',
      '0xa000',
      '@INPUT@',
      '@OUTPUT@',
    ],
    build_subdir: 'podium-dd1-dd2-wqr.elf.p',
  )
  custom_target(
    'podium-dd1-dd2-wqr',
    input: firmware_hex,
    output: 'podium-dd1-dd2-wqr.hex',
    command: [
      python,
      meson.project_source_root() / 'tools/crypto.py',
      'encrypt',
      '@INPUT@',
      '@OUTPUT@',
      '--legacy-wide-secret',
      '--preserve-newlines',
    ],
    build_by_default: true,
  )

  simulator = subproject('kinetis-sim', native: true)
  simulator_firmware_image = simulator.get_variable('kinetis_firmware_image_dependency')
  firmware_test = executable(
    'podium-dd1-dd2-wqr-test',
    'tests/firmware_test.c',
    protocol_sources,
    build_by_default: false,
    dependencies: simulator_firmware_image,
    include_directories: protocol_include,
    native: true,
  )
  test(
    'podium-dd1-dd2-wqr',
    firmware_test,
    args: [firmware_image],
    depends: firmware_image,
    suite: ['wqr', 'firmware'],
  )
else
  protocol_test = executable(
    'wqr-protocol-test',
    'tests/protocol_test.c',
    protocol_sources,
    build_by_default: false,
    include_directories: protocol_include,
  )
  test('wqr-protocol', protocol_test, suite: ['wqr', 'protocol'])
endif
