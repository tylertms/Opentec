if meson.is_cross_build() and host_machine.cpu_family() == 'dspic'
  device_pack = subproject('dspic33e-device-pack')
  device_pack_path = device_pack.get_variable('device_pack_path')
  compiler = find_program('xc16-gcc')
  bin2hex = find_program('xc16-bin2hex')
  device_args = [
    '-mcpu=33EP512MU810',
    '-mdfp=' + device_pack_path,
  ]
  linker_script = device_pack_path / 'support/dsPIC33E/gld/p33EP512MU810.gld'

  firmware_object = custom_target(
    'podium-dd1-dd2-base-object',
    input: 'source/firmware.c',
    output: 'firmware.o',
    command: [compiler] + device_args + ['-c', '@INPUT@', '-o', '@OUTPUT@'],
    build_by_default: false,
  )

  firmware = custom_target(
    'podium-dd1-dd2-base-elf',
    input: firmware_object,
    output: 'podium-dd1-dd2-base.elf',
    command: [compiler] + device_args + [
      '-Wl,--script=' + linker_script,
      '-o',
      '@OUTPUT@',
      '@INPUT@',
    ],
    build_by_default: false,
  )

  custom_target(
    'podium-dd1-dd2-base-hex',
    input: firmware,
    output: 'podium-dd1-dd2-base.hex',
    command: [bin2hex, '@INPUT@'],
    build_by_default: true,
  )
endif
