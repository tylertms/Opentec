motor_target = (
  meson.is_cross_build()
  and host_machine.cpu_family() == 'arm'
  and host_machine.cpu() == 'cortex-m0plus'
  and meson.get_external_property('motor_product', '') == 'dd1'
)

if motor_target
  mcux_sdk = subproject('mcux-sdk')
  cmsis = subproject('cmsis')
  freemaster = subproject('freemaster')
  rtcesl = subproject('rtcesl')
  sdk_drivers = mcux_sdk.get_variable('mcux_sdk_driver_sources')
  sdk_driver_include = mcux_sdk.get_variable('mcux_sdk_driver_includes')
  sdk_device = mcux_sdk.get_variable('mcux_sdk_devices')['MKV10Z1287']
  cmsis_dependency = cmsis.get_variable('cmsis_dependency')
  freemaster_sources = freemaster.get_variable('freemaster_sources')
  freemaster_includes = freemaster.get_variable('freemaster_includes')
  rtcesl_dependency = rtcesl.get_variable('rtcesl_dependency')
  firmware_include = include_directories('source')
  linker_script = files('source/firmware.ld')[0]
  objcopy = find_program('objcopy')
  python = import('python').find_installation()

  sdk_sources = sdk_device['clock']
  foreach driver : [
    'common',
    'adc16',
    'dac',
    'dmamux',
    'edma',
    'ftm',
    'gpio',
    'i2c',
    'pdb',
    'uart',
    'wdog',
  ]
    sdk_sources += sdk_drivers[driver]
  endforeach

  freestanding_c_args = [
    '-ffreestanding',
    '-ffunction-sections',
    '-fdata-sections',
    '-fno-pic',
    '-fno-pie',
    '-DCPU_MKV10Z128VLH7',
    '-DDISABLE_WDOG=1',
    '-DNDEBUG',
  ]

  sdk_library = static_library(
    'podium-dd1-motor-sdk',
    sdk_sources,
    build_by_default: false,
    c_args: freestanding_c_args,
    dependencies: cmsis_dependency,
    include_directories: [sdk_driver_include, sdk_device['includes']],
    override_options: ['warning_level=0'],
    pic: false,
  )

  freemaster_library = static_library(
    'podium-dd1-motor-freemaster',
    freemaster_sources,
    build_by_default: false,
    c_args: freestanding_c_args,
    dependencies: cmsis_dependency,
    include_directories: [firmware_include, freemaster_includes],
    override_options: ['warning_level=0'],
    pic: false,
  )

  firmware = executable(
    'podium-dd1-motor',
    'source/firmware.c',
    sdk_device['sources'],
    build_by_default: false,
    c_args: freestanding_c_args + [
      '-fno-common',
      '-D__START=firmware_main',
      '-D__STARTUP_CLEAR_BSS',
    ],
    dependencies: [cmsis_dependency, rtcesl_dependency],
    include_directories: [
      firmware_include,
      freemaster_includes,
      sdk_driver_include,
      sdk_device['includes'],
    ],
    link_args: [
      '-nostartfiles',
      '-no-pie',
      '-Wl,--gc-sections',
      '-T' + linker_script.full_path(),
    ],
    link_depends: linker_script,
    link_with: [sdk_library, freemaster_library],
    name_suffix: 'elf',
  )

  firmware_binary = custom_target(
    'motor-binary',
    input: firmware,
    output: 'firmware.bin',
    command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
    build_subdir: 'motor-package.p',
  )

  checksummed_binary = custom_target(
    'dd1-motor-checksum',
    input: firmware_binary,
    output: 'dd1-firmware.checksummed.bin',
    command: [
      python,
      meson.project_source_root() / 'tools/motor_checksum.py',
      'dd1',
      '@INPUT@',
      '@OUTPUT@',
    ],
    build_subdir: 'motor-package.p',
  )

  firmware_hex = custom_target(
    'dd1-motor-intel-hex',
    input: checksummed_binary,
    output: 'dd1-firmware.hex',
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
    build_subdir: 'motor-package.p',
  )

  custom_target(
    'podium-dd1-motor',
    input: firmware_hex,
    output: 'podium-dd1-motor.hex',
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
endif
