sources = files('source/protocol.c')
include = include_directories('source')

if meson.is_cross_build()
  linker_script = 'source/vendor/mkv30f12810/gcc/MKV30F128xxx10_flash.ld'

  firmware = executable(
    'podium-dd1-dd2-wqr',
    sources,
    'source/platform/kinetis/platform.c',
    'source/vendor/mkv30f12810/gcc/startup_MKV30F12810.S',
    c_args: [
      '-ffreestanding',
      '-ffunction-sections',
      '-fdata-sections',
      '-fno-common',
      '-D__START=firmware_main',
      '-D__STARTUP_CLEAR_BSS',
    ],
    include_directories: [
      include,
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

  simulator = subproject('kinetis-sim', native: true)
  simulator_image = simulator.get_variable('kinetis_firmware_image_dependency')
  firmware_test = executable(
    'podium-dd1-dd2-wqr-test',
    'tests/firmware_test.c',
    sources,
    native: true,
    dependencies: simulator_image,
    include_directories: include,
  )

  test(
    'podium-dd1-dd2-wqr',
    firmware_test,
    args: [firmware.full_path()],
    suite: ['wqr', 'firmware'],
  )
else
  subdir('tests')
endif
