sources = files('source/protocol.c')
include = include_directories('source')

if meson.is_cross_build()
  linker_script = 'source/vendor/mk22f12810/gcc/MK22FN128xxx10_flash.ld'

  executable(
    'podium-dd1-dd2-wqr',
    sources,
    'source/platform/kinetis/platform.c',
    'source/vendor/mk22f12810/gcc/startup_MK22F12810.S',
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
      include_directories('source/vendor/mk22f12810', is_system: true),
    ],
    link_args: [
      '-nostartfiles',
      '-Wl,--gc-sections',
      '-T' + (meson.current_source_dir() / linker_script),
    ],
    link_depends: files(linker_script),
    name_suffix: 'elf',
  )
else
  subdir('tests')
endif
