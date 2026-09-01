wqr_name = 'podium-dd1-dd2-wqr'
wqr_include = include_directories('source')
wqr_protocol_sources = files('source/protocol.c', 'source/wqr_frame.c')
wqr_sources = wqr_protocol_sources + files('source/firmware.c')
wqr_is_target = (
  meson.is_cross_build()
  and host_machine.cpu_family() == 'arm'
  and host_machine.cpu() == 'cortex-m4'
)

if wqr_is_target
  mcux_sdk = subproject('mcux-sdk')
  cmsis = subproject('cmsis')
  sdk_drivers = mcux_sdk.get_variable('mcux_sdk_driver_sources')
  sdk_driver_include = mcux_sdk.get_variable('mcux_sdk_driver_includes')
  sdk_device = mcux_sdk.get_variable('mcux_sdk_devices')['MKV30F12810']
  cmsis_dependency = cmsis.get_variable('cmsis_dependency')
  linker_script = files('source/firmware.ld')[0]
  objcopy = find_program('objcopy')
  python = import('python').find_installation()
  fs = import('fs')
  wqr_package_dependencies = files('../../tools/crypto.py')
  if fs.exists(meson.project_source_root() / '.env')
    wqr_package_dependencies += files('../../.env')
  endif

  sdk_sources = sdk_device['clock']
  foreach driver : [
    'common',
    'adc16',
    'dmamux',
    'dspi',
    'edma',
    'gpio',
    'i2c',
    'lptmr',
    'pit',
    'smc',
    'uart',
    'uart_edma',
    'wdog',
  ]
    sdk_sources += sdk_drivers[driver]
  endforeach

  wqr_c_args = [
    '-ffreestanding',
    '-ffunction-sections',
    '-fdata-sections',
    '-fno-pic',
    '-fno-pie',
    '-DCPU_MKV30F128VLH10',
    '-DDISABLE_WDOG=1',
    '-DNDEBUG',
  ]
  wqr_includes = [wqr_include, sdk_driver_include, sdk_device['includes']]

  sdk_library = static_library(
    wqr_name + '-sdk',
    sdk_sources,
    build_by_default: false,
    c_args: wqr_c_args,
    dependencies: cmsis_dependency,
    include_directories: [sdk_driver_include, sdk_device['includes']],
    override_options: ['warning_level=0'],
    pic: false,
  )
  firmware = executable(
    wqr_name,
    wqr_sources,
    sdk_device['sources'],
    build_by_default: false,
    c_args: wqr_c_args + [
      '-fno-common',
      '-D__START=firmware_main',
      '-D__STARTUP_CLEAR_BSS',
    ],
    dependencies: cmsis_dependency,
    include_directories: wqr_includes,
    link_args: [
      '-nostartfiles',
      '-no-pie',
      '-Wl,--gc-sections',
      '-T' + linker_script.full_path(),
    ],
    link_depends: linker_script,
    link_with: sdk_library,
    name_suffix: 'elf',
    build_subdir: 'package.p',
  )

  binary = custom_target(
    wqr_name + '-binary',
    input: firmware,
    output: 'firmware.bin',
    command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
    build_subdir: 'package.p',
  )
  checksummed_binary = custom_target(
    wqr_name + '-checksum',
    input: binary,
    output: 'firmware.checksummed.bin',
    command: [
      python,
      meson.current_source_dir() / 'tools/checksum.py',
      '@INPUT@',
      '@OUTPUT@',
    ],
    depend_files: files('tools/checksum.py'),
    build_subdir: 'package.p',
  )
  hex = custom_target(
    wqr_name + '-intel-hex',
    input: checksummed_binary,
    output: 'firmware.hex',
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
endif

if not meson.is_cross_build() or wqr_is_target
  subdir('tests')
endif
