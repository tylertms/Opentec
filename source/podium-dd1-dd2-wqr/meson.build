protocol_sources = files('source/protocol.c')
protocol_include = include_directories('source')

if meson.is_cross_build()
  linker_script = 'source/vendor/mkv30f12810/gcc/MKV30F128xxx10_flash.ld'

  firmware = executable(
    'podium-dd1-dd2-wqr',
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
    link_args: [
      '-nostartfiles',
      '-Wl,--gc-sections',
      '-T' + (meson.current_source_dir() / linker_script),
    ],
    link_depends: files(linker_script),
    name_suffix: 'elf',
  )
  firmware_image = custom_target(
    'podium-dd1-dd2-wqr-image',
    input: firmware,
    output: 'podium-dd1-dd2-wqr.bin',
    command: [
      import('python').find_installation(),
      meson.project_source_root() / 'tools/wqr_checksum.py',
      '@INPUT@',
      '@OUTPUT@',
    ],
    build_by_default: true,
  )

  simulator = subproject('kinetis-sim', native: true)
  simulator_firmware_image = simulator.get_variable('kinetis_firmware_image_dependency')
  firmware_test = executable(
    'podium-dd1-dd2-wqr-test',
    'tests/firmware_test.c',
    protocol_sources,
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
  subdir('tests')
endif
