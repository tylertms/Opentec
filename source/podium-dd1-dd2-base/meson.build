base_include = include_directories('source')
base_source_path = meson.current_source_dir() / 'source'
common_source_path = meson.project_source_root() / 'source'
analog_axis_sources = files('source/analog/axis.c')
force_output_sources = files('source/force_feedback/output.c')
remote_exchange_sources = files('source/wheel/remote_exchange.c')
remote_i2c_sources = files('source/wheel/remote_i2c.c')
remote_spi_sources = files('source/wheel/remote_spi.c')
remote_status_sources = files('source/wheel/remote_status.c')
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
    'analog-axis': 'source/analog/axis.c',
    'force-output': 'source/force_feedback/output.c',
    'firmware': 'source/firmware.c',
    'remote-exchange': 'source/wheel/remote_exchange.c',
    'remote-i2c': 'source/wheel/remote_i2c.c',
    'remote-spi': 'source/wheel/remote_spi.c',
    'remote-status': 'source/wheel/remote_status.c',
    'wqr-frame': meson.project_source_root() / 'source/common/wqr_frame.c',
    'wheel-position': 'source/wheel/position.c',
  }
    firmware_objects += custom_target(
      'podium-dd1-dd2-base-' + name + '-object',
      input: source,
      output: name + '.o',
      command: [compiler] + device_args + [
        '-I' + base_source_path,
        '-I' + common_source_path,
        '-c',
        '@INPUT@',
        '-o',
        '@OUTPUT@',
      ],
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
  analog_axis_test = executable(
    'base-analog-axis-test',
    'tests/source/analog_axis.c',
    analog_axis_sources,
    build_by_default: false,
    include_directories: base_include,
  )

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

  remote_exchange_test = executable(
    'base-remote-exchange-test',
    'tests/source/remote_exchange.c',
    remote_exchange_sources,
    wqr_frame_sources,
    build_by_default: false,
    include_directories: [base_include, common_include],
  )

  remote_status_test = executable(
    'base-remote-status-test',
    'tests/source/remote_status.c',
    remote_exchange_sources,
    remote_status_sources,
    wqr_frame_sources,
    build_by_default: false,
    include_directories: [base_include, common_include],
  )

  remote_i2c_test = executable(
    'base-remote-i2c-test',
    'tests/source/remote_i2c.c',
    remote_exchange_sources,
    remote_i2c_sources,
    wqr_frame_sources,
    build_by_default: false,
    include_directories: [base_include, common_include],
  )

  remote_spi_test = executable(
    'base-remote-spi-test',
    'tests/source/remote_spi.c',
    remote_exchange_sources,
    remote_spi_sources,
    wqr_frame_sources,
    build_by_default: false,
    include_directories: [base_include, common_include],
  )

  wqr_frame_test = executable(
    'base-wqr-frame-test',
    'tests/source/wqr_frame.c',
    wqr_frame_sources,
    build_by_default: false,
    include_directories: common_include,
  )

  test('base-analog-axis', analog_axis_test, suite: ['base', 'analog'])
  test('base-force-output', force_output_test, suite: ['base', 'force-feedback'])
  test('base-remote-exchange', remote_exchange_test, suite: ['base', 'transport'])
  test('base-remote-i2c', remote_i2c_test, suite: ['base', 'wheel-i2c'])
  test('base-remote-spi', remote_spi_test, suite: ['base', 'wheel-spi'])
  test('base-remote-status', remote_status_test, suite: ['base', 'wheel-status'])
  test('base-wheel-position', wheel_position_test, suite: ['base', 'wheel-position'])
  test('base-wqr-frame', wqr_frame_test, suite: ['base', 'transport'])
endif
