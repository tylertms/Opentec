motor_name = 'podium-dd1-motor'
motor_product = 'dd1'
motor_include = include_directories('source')
motor_sources = files(
  'source/firmware.c',
  'source/product.c',
  'source/platform/mkv10z/board.c',
  'source/platform/mkv10z/tuning_bus.c',
  'source/motor/foc.c',
  'source/motor/pi.c',
  'source/platform/mkv10z/io.c',
  'source/platform/mkv10z/link.c',
  'source/platform/mkv10z/peripherals.c',
  'source/system/runtime.c',
  'source/platform/mkv10z/spi.c',
  'source/motor/center.c',
  'source/motor/control.c',
  'source/motor/calibration.c',
  'source/motor/drive.c',
  'source/motor/encoder.c',
  'source/motor/encoder_calibration.c',
  'source/platform/mkv10z/storage.c',
  'source/force_feedback/effect.c',
  'source/force_feedback/command.c',
  'source/force_feedback/engine.c',
  'source/force_feedback/soft_stop.c',
  'source/profile/hardware.c',
  'source/link/frame.c',
  'source/motor/motion.c',
  'source/tuning/parameter.c',
  'source/link/protocol.c',
  'source/system/timing.c',
  'source/telemetry/auxiliary.c',
  'source/motor/velocity_control.c',
)

motor_is_target = (
  meson.is_cross_build()
  and host_machine.cpu_family() == 'arm'
  and host_machine.cpu() == 'cortex-m0plus'
  and meson.get_external_property('motor_product', '') == motor_product
)

if motor_is_target
  if meson.get_compiler('c').version() != '15.3.1'
    error('Arm GNU Toolchain 15.3.1 is required for release builds')
  endif
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
  fs = import('fs')
  package_dependencies = files('../../tools/crypto.py')
  if fs.exists(meson.project_source_root() / '.env')
    package_dependencies += files('../../.env')
  endif

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

  motor_c_args = [
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
  motor_includes = [
    motor_include,
    freemaster_includes,
    sdk_driver_include,
    sdk_device['includes'],
  ]

  sdk_library = static_library(
    motor_name + '-sdk',
    sdk_sources,
    build_by_default: false,
    c_args: motor_c_args,
    dependencies: cmsis_dependency,
    include_directories: [sdk_driver_include, sdk_device['includes']],
    override_options: ['warning_level=0'],
    pic: false,
  )
  freemaster_library = static_library(
    motor_name + '-freemaster',
    freemaster_sources,
    build_by_default: false,
    c_args: motor_c_args,
    dependencies: cmsis_dependency,
    include_directories: motor_includes,
    override_options: ['warning_level=0'],
    pic: false,
  )
  firmware = executable(
    motor_name,
    motor_sources,
    sdk_device['sources'],
    build_by_default: false,
    c_args: motor_c_args + [
      '-fno-common',
      '-D__START=firmware_main',
      '-D__STARTUP_CLEAR_BSS',
    ],
    dependencies: [cmsis_dependency, rtcesl_dependency],
    include_directories: motor_includes,
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

  motor_target_test_sources = files(
    'tests/source/target.c',
    'tests/source/current_calibration.c',
    'tests/source/control.c',
    'tests/source/pi.c',
    'tests/source/foc.c',
    'tests/source/velocity_control.c',
    'tests/source/center.c',
    'tests/source/encoder.c',
    'tests/source/encoder_calibration.c',
    'tests/source/service_timing.c',
    'tests/source/force_feedback.c',
    'tests/source/link_frame.c',
    'tests/source/drive.c',
    'tests/source/hardware_profile.c',
    'tests/source/motion.c',
    'tests/source/telemetry.c',
    'tests/source/parameter.c',
    'tests/source/protocol.c',
  )
  target_tests = executable(
    motor_name + '-target-tests',
    motor_sources,
    motor_target_test_sources,
    sdk_device['sources'],
    build_by_default: false,
    c_args: motor_c_args + [
      '-UNDEBUG',
      '-fno-common',
      '-D__START=motor_test_main',
      '-D__STARTUP_CLEAR_BSS',
    ],
    dependencies: [cmsis_dependency, rtcesl_dependency],
    include_directories: motor_includes,
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

  storage_tests = executable(
    motor_name + '-storage-tests',
    'source/motor/encoder_calibration.c',
    'source/platform/mkv10z/storage.c',
    'tests/source/storage.c',
    'tests/source/storage_target.c',
    sdk_device['sources'],
    build_by_default: false,
    c_args: motor_c_args + [
      '-UNDEBUG',
      '-fno-common',
      '-D__START=motor_storage_test_main',
      '-D__STARTUP_CLEAR_BSS',
    ],
    dependencies: [cmsis_dependency, rtcesl_dependency],
    include_directories: motor_includes,
    link_args: [
      '-nostartfiles',
      '-no-pie',
      '-Wl,--gc-sections',
      '-T' + linker_script.full_path(),
    ],
    link_depends: linker_script,
    link_with: sdk_library,
    name_suffix: 'elf',
  )

  simulator = subproject('kinetis-sim', native: true)
  simulator_runner = simulator.get_variable('kinetis_firmware_runner')
  simulator_image = simulator.get_variable('kinetis_firmware_image_dependency')
  scenario = executable(
    motor_name + '-firmware-scenario',
    'tests/source/firmware_simulation.c',
    native: true,
    build_by_default: false,
    dependencies: simulator_image,
  )
  test(
    motor_name + '-firmware',
    simulator_runner,
    args: [
      firmware.full_path(),
      '--profile',
      'MKV10Z1287',
      '--package',
      'LH',
      '--reset-address',
      '0xa000',
      '--max-instructions',
      '1000000',
      '--max-cycles',
      '15000000',
      '--minimum-instructions-covered',
      '3500',
      '--minimum-branches-covered',
      '208',
      '--expect-instruction-limit',
    ],
    suite: ['motor', 'simulator'],
    depends: firmware,
    timeout: 120,
  )
  test(
    motor_name + '-firmware-scenario',
    scenario,
    args: firmware.full_path(),
    suite: ['motor', 'simulator'],
    depends: firmware,
    timeout: 300,
  )
  test(
    motor_name + '-target-tests',
    simulator_runner,
    args: [
      target_tests.full_path(),
      '--profile',
      'MKV10Z1287',
      '--package',
      'LH',
      '--reset-address',
      '0xa000',
      '--max-instructions',
      '150000000',
      '--max-cycles',
      '500000000',
      '--expect-instruction-limit',
    ],
    suite: ['motor', 'simulator'],
    depends: target_tests,
    timeout: 300,
  )
  test(
    motor_name + '-storage-tests',
    simulator_runner,
    args: [
      storage_tests.full_path(),
      '--profile',
      'MKV10Z1287',
      '--package',
      'LH',
      '--reset-address',
      '0xa000',
      '--max-instructions',
      '5000000',
      '--max-cycles',
      '100000000',
      '--expect-instruction-limit',
    ],
    suite: ['motor', 'simulator'],
    depends: storage_tests,
    timeout: 120,
  )

  binary = custom_target(
    motor_name + '-binary',
    input: firmware,
    output: 'firmware.bin',
    command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
    build_subdir: 'package.p',
  )
  checksummed_binary = custom_target(
    motor_name + '-checksum',
    input: binary,
    output: motor_product + '-firmware.checksummed.bin',
    command: [
      python,
      meson.current_source_dir() / 'tools/checksum.py',
      motor_product,
      '@INPUT@',
      '@OUTPUT@',
    ],
    depend_files: files('tools/checksum.py'),
    build_subdir: 'package.p',
  )
  hex = custom_target(
    motor_name + '-intel-hex',
    input: checksummed_binary,
    output: motor_product + '-firmware.hex',
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
    build_subdir: 'package.p',
  )
  custom_target(
    motor_name,
    input: hex,
    output: motor_name + '.hex',
    command: [
      python,
      meson.project_source_root() / 'tools/crypto.py',
      'encrypt',
      '@INPUT@',
      '@OUTPUT@',
      '--legacy-wide-secret',
      '--preserve-newlines',
    ],
    depend_files: package_dependencies,
    build_by_default: true,
    build_always_stale: true,
  )
endif
