protocol_sources = files('source/protocol.c')
protocol_include = include_directories('source')

if meson.is_cross_build()
  linker_script = 'source/vendor/mkv30f12810/gcc/MKV30F128xxx10_flash.ld'
  objcopy = find_program('objcopy')
  python = import('python').find_installation()

  firmware = executable(
    'podium-dd1-dd2-wqr.elf',
    protocol_sources,
    'source/platform/kinetis/platform.c',
    'source/vendor/mkv30f12810/gcc/startup_MKV30F12810.S',
    c_args: [
      '-ffreestanding',
      '-ffunction-sections',
      '-fdata-sections',
      '-fno-common',
      '-DCPU_MKV30F128VLH10',
      '-D__START=firmware_main',
      '-D__STARTUP_CLEAR_BSS',
    ],
    include_directories: [
      protocol_include,
      include_directories('source/vendor/cmsis-core/include', is_system: true),
      include_directories('source/vendor/mkv30f12810', is_system: true),
    ],
    build_by_default: false,
    link_args: [
      '-nostartfiles',
      '-Wl,--gc-sections',
      '-T' + (meson.current_source_dir() / linker_script),
    ],
    link_depends: files(linker_script),
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
    native: true,
    dependencies: simulator_firmware_image,
    include_directories: protocol_include,
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
