base_include = include_directories('source')
base_source_path = meson.current_source_dir() / 'source'
force_output_sources = files('source/force_feedback/output.c')
wheel_position_sources = files('source/wheel/position.c')

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

  firmware_objects = []
  foreach name, source : {
    'force-output': 'source/force_feedback/output.c',
    'firmware': 'source/firmware.c',
    'wheel-position': 'source/wheel/position.c',
  }
    firmware_objects += custom_target(
      'podium-dd1-dd2-base-' + name + '-object',
      input: source,
      output: name + '.o',
      command: [compiler] + device_args + ['-I' + base_source_path, '-c', '@INPUT@', '-o', '@OUTPUT@'],
      build_by_default: false,
    )
  endforeach

  firmware = custom_target(
    'podium-dd1-dd2-base-elf',
    input: firmware_objects,
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

if not meson.is_cross_build()
  force_output_test = executable(
    'base-force-output-test',
    'tests/source/force_output.c',
    force_output_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_position_test = executable(
    'base-wheel-position-test',
    'tests/source/wheel_position.c',
    wheel_position_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  test('base-force-output', force_output_test, suite: ['base', 'force-feedback'])
  test('base-wheel-position', wheel_position_test, suite: ['base', 'wheel-position'])
endif
