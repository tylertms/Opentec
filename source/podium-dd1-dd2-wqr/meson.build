protocol_include = include_directories('source')
protocol_options = []

if meson.is_cross_build()
  protocol_options = ['optimization=2', 'debug=false']
endif

protocol_library = static_library(
  'wqr-protocol',
  'source/protocol.c',
  include_directories: protocol_include,
  override_options: protocol_options,
)

protocol_dependency = declare_dependency(
  include_directories: protocol_include,
  link_with: protocol_library,
)

if meson.is_cross_build()
  arm_objcopy = find_program('arm-none-eabi-objcopy')
  python = import('python').find_installation()
  linker_script = files('MK22FN128xxx10_flash.ld')
  device_include = include_directories(
    'source/vendor/cmsis-core/include',
    'source/vendor/mk22f12810',
  )
  firmware = executable(
    'podium-dd1-dd2-wqr',
    'source/vendor/mk22f12810/gcc/startup_MK22F12810.S',
    'source/vendor/mk22f12810/system_MK22F12810.c',
    'source/platform/kinetis/platform.c',
    c_args: [
      '-DCPU_MK22FN128VLH10',
      '-D__START=firmware_main',
      '-D__STARTUP_CLEAR_BSS',
    ],
    dependencies: protocol_dependency,
    include_directories: device_include,
    override_options: ['warning_level=2', 'optimization=2', 'debug=false'],
    link_args: [
      '-nostdlib',
      '-Wl,--gc-sections',
      '-Wl,-Map=' + (meson.current_build_dir() / 'podium-dd1-dd2-wqr.map'),
      '-T' + (meson.current_source_dir() / 'MK22FN128xxx10_flash.ld'),
      '-lc',
      '-lgcc',
    ],
    link_depends: linker_script,
    name_suffix: 'elf',
  )

  firmware_binary = custom_target(
    'podium-dd1-dd2-wqr-binary',
    input: firmware,
    output: 'podium-dd1-dd2-wqr.unchecked.bin',
    command: [arm_objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
  )

  checked_binary = custom_target(
    'podium-dd1-dd2-wqr-checksum',
    input: firmware_binary,
    output: 'podium-dd1-dd2-wqr.bin',
    command: [
      python,
      meson.project_source_root() / 'tools' / 'wqr_checksum.py',
      '@INPUT@',
      '@OUTPUT@',
    ],
  )

  custom_target(
    'podium-dd1-dd2-wqr-hex',
    input: checked_binary,
    output: 'podium-dd1-dd2-wqr.hex',
    command: [
      arm_objcopy,
      '-I', 'binary',
      '-O', 'ihex',
      '--change-addresses', '0xa000',
      '@INPUT@',
      '@OUTPUT@',
    ],
    build_by_default: true,
  )
else
  protocol_test = executable(
    'wqr-protocol-test',
    'tests/protocol_test.c',
    dependencies: protocol_dependency,
  )

  test('wqr-protocol', protocol_test)
endif
