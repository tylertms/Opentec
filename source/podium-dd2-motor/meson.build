firmware_include = include_directories('source')
motor_center_sources = files('source/motor/center.c')
motor_control_state_sources = files('source/motor/control.c')
motor_current_offset_calibration_sources = files('source/motor/calibration.c')
motor_drive_sources = files('source/motor/drive.c')
motor_encoder_sources = files('source/motor/encoder.c')
motor_encoder_calibration_sources = files('source/motor/encoder_calibration.c')
motor_calibration_storage_sources = files('source/platform/mkv10z/storage.c')
motor_force_feedback_sources = files(
  'source/force_feedback/effect.c',
  'source/force_feedback/command.c',
  'source/force_feedback/engine.c',
  'source/force_feedback/soft_stop.c',
)
motor_hardware_profile_sources = files('source/profile/hardware.c')
motor_link_frame_sources = files('source/link/frame.c')
motor_motion_sources = files('source/motor/motion.c')
motor_parameter_sources = files('source/tuning/parameter.c')
motor_protocol_sources = files('source/link/protocol.c')
motor_service_sources = files('source/system/timing.c')
motor_auxiliary_telemetry_sources = files('source/telemetry/auxiliary.c')
motor_velocity_control_sources = files('source/motor/velocity_control.c')
motor_control_sources = files(
  'source/platform/mkv10z/board.c',
  'source/platform/mkv10z/tuning_bus.c',
  'source/motor/foc.c',
  'source/platform/mkv10z/io.c',
  'source/platform/mkv10z/link.c',
  'source/platform/mkv10z/peripherals.c',
  'source/system/runtime.c',
  'source/platform/mkv10z/spi.c',
) + motor_center_sources + motor_control_state_sources + motor_current_offset_calibration_sources + motor_drive_sources + motor_encoder_sources + motor_encoder_calibration_sources + motor_calibration_storage_sources + motor_force_feedback_sources + motor_hardware_profile_sources + motor_link_frame_sources + motor_motion_sources + motor_parameter_sources + motor_protocol_sources + motor_service_sources + motor_auxiliary_telemetry_sources + motor_velocity_control_sources


motor_target = (
  meson.is_cross_build()
  and host_machine.cpu_family() == 'arm'
  and host_machine.cpu() == 'cortex-m0plus'
  and meson.get_external_property('motor_product', '') == 'dd2'
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
  linker_script = files('source/firmware.ld')[0]
  objcopy = find_program('objcopy')
  python = import('python').find_installation()

  sdk_sources = sdk_device['clock']
  foreach driver : [
    'common',
    'adc16',
    'crc',
    'dac',
    'dmamux',
    'edma',
    'flash',
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
    '-DRTCESL_MMDVSQ_ON',
  ]

  sdk_library = static_library(
    'podium-dd2-motor-sdk',
    sdk_sources,
    build_by_default: false,
    c_args: freestanding_c_args,
    dependencies: cmsis_dependency,
    include_directories: [sdk_driver_include, sdk_device['includes']],
    override_options: ['warning_level=0'],
    pic: false,
  )

  freemaster_library = static_library(
    'podium-dd2-motor-freemaster',
    freemaster_sources,
    build_by_default: false,
    c_args: freestanding_c_args,
    dependencies: cmsis_dependency,
    include_directories: [
      firmware_include,
      freemaster_includes,
      sdk_driver_include,
      sdk_device['includes'],
    ],
    override_options: ['warning_level=0'],
    pic: false,
  )

  firmware = executable(
    'podium-dd2-motor',
    'source/firmware.c',
    'source/product.c',
    motor_control_sources,
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

  motor_simulator = subproject('kinetis-sim', native: true)
  motor_runner = motor_simulator.get_variable('kinetis_firmware_runner')
  test(
    'dd2-motor-firmware',
    motor_runner,
    args: [
      firmware.full_path(),
      '--profile',
      'MKV10Z1287',
      '--package',
      'LH',
      '--reset-address',
      '0xa000',
      '--write32',
      '0x40048034:0xf0000430',
      '--max-instructions',
      '1000000',
      '--max-cycles',
      '15000000',
      '--minimum-instructions-covered',
      '8000',
      '--minimum-branches-covered',
      '250',
    ],
    suite: ['motor', 'simulator'],
    timeout: 120,
  )

  firmware_binary = custom_target(
    'motor-binary',
    input: firmware,
    output: 'firmware.bin',
    command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
    build_subdir: 'motor-package.p',
  )

  checksummed_binary = custom_target(
    'dd2-motor-checksum',
    input: firmware_binary,
    output: 'dd2-firmware.checksummed.bin',
    command: [
      python,
      meson.current_source_dir() / 'tools/checksum.py',
      'dd2',
      '@INPUT@',
      '@OUTPUT@',
    ],
    build_subdir: 'motor-package.p',
  )

  firmware_hex = custom_target(
    'dd2-motor-intel-hex',
    input: checksummed_binary,
    output: 'dd2-firmware.hex',
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
    'podium-dd2-motor',
    input: firmware_hex,
    output: 'podium-dd2-motor.hex',
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

if not meson.is_cross_build()
  motor_test_prefix = 'dd2-motor'
  subdir('tests')
endif
