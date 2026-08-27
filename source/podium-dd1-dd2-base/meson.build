base_include = include_directories('source')
base_source_path = meson.current_source_dir() / 'source'
common_source_path = meson.project_source_root() / 'source'
analog_axis_sources = files('source/analog/axis.c')
analog_samples_sources = files('source/analog/samples.c')
force_effect_bank_sources = files('source/force_feedback/bank.c')
force_effect_sources = files('source/force_feedback/effect.c')
force_output_sources = files('source/force_feedback/output.c')
force_output_report_sources = files('source/force_feedback/output_report.c')
force_filter_sources = files('source/force_feedback/filter.c')
force_soft_stop_sources = files('source/force_feedback/soft_stop.c')
fanatec_input_sources = files('source/usb/fanatec_input.c')
button_filter_sources = files('source/input/button_filter.c')
board_identity_sources = files('source/board/identity.c')
h_pattern_shifter_sources = files('source/shifter/h_pattern.c')
logitech_input_sources = files('source/usb/logitech_input.c')
motor_tuning_parameter_sources = files('source/motor/tuning_parameter.c')
motor_tuning_service_sources = files('source/motor/tuning_service.c')
motor_tuning_sync_sources = files('source/motor/tuning_sync.c')
pedal_frame_sources = files('source/pedal/frame.c')
pedal_axis_sources = files('source/pedal/axis.c')
profile_bank_sources = files('source/profile/bank.c')
profile_record_sources = files('source/profile/record.c')
profile_tuning_sources = files('source/profile/tuning.c')
platform_time_sources = files('source/platform/time.c')
shifter_frame_sources = files('source/shifter/frame.c')
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
    'analog-samples': 'source/analog/samples.c',
    'force-effect-bank': 'source/force_feedback/bank.c',
    'force-effect': 'source/force_feedback/effect.c',
    'force-output': 'source/force_feedback/output.c',
    'force-output-report': 'source/force_feedback/output_report.c',
    'force-filter': 'source/force_feedback/filter.c',
    'force-soft-stop': 'source/force_feedback/soft_stop.c',
    'fanatec-input': 'source/usb/fanatec_input.c',
    'button-filter': 'source/input/button_filter.c',
    'board-identity': 'source/board/identity.c',
    'firmware': 'source/firmware.c',
    'h-pattern-shifter': 'source/shifter/h_pattern.c',
    'logitech-input': 'source/usb/logitech_input.c',
    'motor-tuning-parameter': 'source/motor/tuning_parameter.c',
    'motor-tuning-service': 'source/motor/tuning_service.c',
    'motor-tuning-sync': 'source/motor/tuning_sync.c',
    'pedal-frame': 'source/pedal/frame.c',
    'pedal-axis': 'source/pedal/axis.c',
    'platform-clock': 'source/platform/dspic33e/clock.c',
    'platform-board-identity': 'source/platform/dspic33e/board_identity.c',
    'platform-adc': 'source/platform/dspic33e/adc.c',
    'platform-aux-bus': 'source/platform/dspic33e/aux_bus.c',
    'platform-pin-mux': 'source/platform/dspic33e/pin_mux.c',
    'platform-time': 'source/platform/dspic33e/time.c',
    'platform-time-common': 'source/platform/time.c',
    'profile-bank': 'source/profile/bank.c',
    'profile-record': 'source/profile/record.c',
    'profile-tuning': 'source/profile/tuning.c',
    'shifter-frame': 'source/shifter/frame.c',
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

  analog_samples_test = executable(
    'base-analog-samples-test',
    'tests/source/analog_samples.c',
    analog_samples_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_effect_test = executable(
    'base-force-effect-test',
    'tests/source/force_effect.c',
    force_effect_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_effect_bank_test = executable(
    'base-force-effect-bank-test',
    'tests/source/force_effect_bank.c',
    force_effect_bank_sources,
    force_effect_sources,
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

  force_output_report_test = executable(
    'base-force-output-report-test',
    'tests/source/force_output_report.c',
    force_output_report_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_filter_test = executable(
    'base-force-filter-test',
    'tests/source/force_filter.c',
    force_filter_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_soft_stop_test = executable(
    'base-force-soft-stop-test',
    'tests/source/force_soft_stop.c',
    force_soft_stop_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  fanatec_input_test = executable(
    'base-fanatec-input-test',
    'tests/source/fanatec_input.c',
    fanatec_input_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  button_filter_test = executable(
    'base-button-filter-test',
    'tests/source/button_filter.c',
    button_filter_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  board_identity_test = executable(
    'base-board-identity-test',
    'tests/source/board_identity.c',
    board_identity_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  h_pattern_shifter_test = executable(
    'base-h-pattern-shifter-test',
    'tests/source/h_pattern_shifter.c',
    h_pattern_shifter_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  logitech_input_test = executable(
    'base-logitech-input-test',
    'tests/source/logitech_input.c',
    logitech_input_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_tuning_parameter_test = executable(
    'base-motor-tuning-parameter-test',
    'tests/source/motor_tuning_parameter.c',
    motor_tuning_parameter_sources,
    profile_tuning_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_tuning_sync_test = executable(
    'base-motor-tuning-sync-test',
    'tests/source/motor_tuning_sync.c',
    motor_tuning_sync_sources,
    motor_tuning_parameter_sources,
    profile_tuning_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_tuning_service_test = executable(
    'base-motor-tuning-service-test',
    'tests/source/motor_tuning_service.c',
    motor_tuning_service_sources,
    motor_tuning_sync_sources,
    motor_tuning_parameter_sources,
    profile_tuning_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  pedal_frame_test = executable(
    'base-pedal-frame-test',
    'tests/source/pedal_frame.c',
    pedal_frame_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  pedal_axis_test = executable(
    'base-pedal-axis-test',
    'tests/source/pedal_axis.c',
    pedal_axis_sources,
    analog_axis_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  profile_tuning_test = executable(
    'base-profile-tuning-test',
    'tests/source/tuning_profile.c',
    profile_tuning_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  profile_bank_test = executable(
    'base-profile-bank-test',
    'tests/source/tuning_profile_bank.c',
    profile_bank_sources,
    profile_tuning_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  profile_record_test = executable(
    'base-profile-record-test',
    'tests/source/tuning_profile_record.c',
    profile_record_sources,
    profile_bank_sources,
    profile_tuning_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  platform_time_test = executable(
    'base-platform-time-test',
    'tests/source/platform_time.c',
    platform_time_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  shifter_frame_test = executable(
    'base-shifter-frame-test',
    'tests/source/shifter_frame.c',
    shifter_frame_sources,
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
  test('base-analog-samples', analog_samples_test, suite: ['base', 'analog'])
  test('base-force-effect', force_effect_test, suite: ['base', 'force-feedback'])
  test('base-force-effect-bank', force_effect_bank_test, suite: ['base', 'force-feedback'])
  test('base-force-output', force_output_test, suite: ['base', 'force-feedback'])
  test(
    'base-force-output-report',
    force_output_report_test,
    suite: ['base', 'force-feedback', 'transport'],
  )
  test('base-force-filter', force_filter_test, suite: ['base', 'force-feedback'])
  test('base-force-soft-stop', force_soft_stop_test, suite: ['base', 'force-feedback'])
  test('base-fanatec-input', fanatec_input_test, suite: ['base', 'usb', 'input'])
  test('base-button-filter', button_filter_test, suite: ['base', 'wheel-input'])
  test('base-board-identity', board_identity_test, suite: ['base', 'board'])
  test('base-h-pattern-shifter', h_pattern_shifter_test, suite: ['base', 'shifter'])
  test('base-logitech-input', logitech_input_test, suite: ['base', 'usb', 'input'])
  test('base-motor-tuning-parameter', motor_tuning_parameter_test, suite: ['base', 'motor'])
  test('base-motor-tuning-sync', motor_tuning_sync_test, suite: ['base', 'motor'])
  test('base-motor-tuning-service', motor_tuning_service_test, suite: ['base', 'motor'])
  test('base-pedal-frame', pedal_frame_test, suite: ['base', 'pedal', 'transport'])
  test('base-pedal-axis', pedal_axis_test, suite: ['base', 'pedal', 'analog'])
  test('base-profile-tuning', profile_tuning_test, suite: ['base', 'profile'])
  test('base-profile-bank', profile_bank_test, suite: ['base', 'profile'])
  test('base-profile-record', profile_record_test, suite: ['base', 'profile', 'persistence'])
  test('base-platform-time', platform_time_test, suite: ['base', 'platform'])
  test('base-shifter-frame', shifter_frame_test, suite: ['base', 'shifter', 'transport'])
  test('base-remote-exchange', remote_exchange_test, suite: ['base', 'transport'])
  test('base-remote-i2c', remote_i2c_test, suite: ['base', 'wheel-i2c'])
  test('base-remote-spi', remote_spi_test, suite: ['base', 'wheel-spi'])
  test('base-remote-status', remote_status_test, suite: ['base', 'wheel-status'])
  test('base-wheel-position', wheel_position_test, suite: ['base', 'wheel-position'])
  test('base-wqr-frame', wqr_frame_test, suite: ['base', 'transport'])
endif
