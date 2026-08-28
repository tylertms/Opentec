base_include = include_directories('source')
base_source_path = meson.current_source_dir() / 'source'
common_source_path = meson.project_source_root() / 'source'
analog_axis_sources = files('source/analog/axis.c')
analog_samples_sources = files('source/analog/samples.c')
cooling_controller_sources = files('source/cooling/controller.c')
cooling_effect_limit_sources = files('source/cooling/effect_limit.c')
cooling_pwm_sources = files('source/cooling/pwm.c')
cooling_tachometer_sources = files('source/cooling/tachometer.c')
cooling_temperature_sources = files('source/cooling/temperature.c')
force_effect_bank_sources = files('source/force_feedback/bank.c')
force_controller_sources = files('source/force_feedback/controller.c')
force_effect_sources = files('source/force_feedback/effect.c')
force_output_sources = files('source/force_feedback/output.c')
force_output_report_sources = files('source/force_feedback/output_report.c')
force_script_control_sources = files('source/force_feedback/script_control.c')
force_filter_sources = files('source/force_feedback/filter.c')
force_safety_sources = files('source/force_feedback/safety.c')
force_soft_stop_sources = files('source/force_feedback/soft_stop.c')
fanatec_input_sources = files('source/usb/fanatec_input.c')
usb_descriptor_sources = files('source/usb/descriptor.c')
usb_control_request_sources = files('source/usb/control_request.c')
usb_control_pipe_sources = files('source/usb/control_pipe.c')
usb_buffer_descriptor_sources = files('source/usb/buffer_descriptor.c')
usb_device_control_sources = files('source/usb/device_control.c')
usb_device_sources = files('source/usb/device.c')
usb_output_command_sources = files('source/usb/output_command.c')
usb_operating_mode_command_sources = files('source/usb/operating_mode_command.c')
usb_vendor_command_sources = files('source/usb/vendor_command.c')
podium_report_descriptor_sources = files('source/usb/podium_report_descriptor.c')
board_identity_sources = files('source/board/identity.c')
power_button_sources = files('source/board/power_button.c')
status_led_sources = files('source/board/status_led.c')
h_pattern_shifter_sources = files('source/shifter/h_pattern.c')
shifter_display_sources = files('source/shifter/display.c')
logitech_input_sources = files('source/usb/logitech_input.c')
motor_identity_sources = files('source/motor/identity.c')
motor_live_frame_sources = files('source/motor/live_frame.c')
motor_output_interlock_sources = files('source/motor/output_interlock.c')
motor_probe_sources = files('source/motor/probe.c')
motor_telemetry_sources = files('source/motor/telemetry.c')
motor_telemetry_service_sources = files('source/motor/telemetry_service.c')
motor_tuning_parameter_sources = files('source/motor/tuning_parameter.c')
motor_tuning_service_sources = files('source/motor/tuning_service.c')
motor_tuning_sync_sources = files('source/motor/tuning_sync.c')
pedal_frame_sources = files('source/pedal/frame.c')
pedal_axis_sources = files('source/pedal/axis.c')
pedal_analog_sources = files('source/pedal/analog.c')
pedal_input_sources = files('source/pedal/input.c')
pedal_protocol_sources = files('source/pedal/protocol.c')
pedal_service_sources = files('source/pedal/service.c')
pedal_v4_status_sources = files('source/pedal/v4_status.c')
pedal_v4_tuning_sources = files('source/pedal/v4_tuning.c')
transfer_frame_sources = files('source/transfer/frame.c')
transfer_session_sources = files('source/transfer/session.c')
profile_bank_sources = files('source/profile/bank.c')
profile_record_sources = files('source/profile/record.c')
profile_tuning_sources = files('source/profile/tuning.c')
settings_persistence_sources = files('source/settings/persistence.c')
settings_state_sources = files('source/settings/state.c')
platform_time_sources = files('source/platform/time.c')
wheel_position_sources = files('source/wheel/position.c')
wheel_axis_override_sources = files('source/wheel/axis_override.c')
wheel_authentication_sources = files('source/wheel/authentication.c')
wheel_display_output_sources = files('source/wheel/display_output.c')
wheel_packet_mode_one_sources = files('source/wheel/packet_mode_one.c')
wheel_protocol_sources = files('source/wheel/protocol.c')
wheel_service_sources = files('source/wheel/service.c')
wheel_transport_frame_sources = files('source/wheel/transport_frame.c')
wheel_transport_service_sources = files('source/wheel/transport_service.c')

if meson.is_cross_build() and host_machine.cpu_family() == 'dspic'
  device_pack = subproject('dspic33e-device-pack')
  device_pack_path = device_pack.get_variable('device_pack_path')
  compiler = find_program('xc16-gcc')
  bin2hex = find_program('xc16-bin2hex')
  device_args = [
    '-mcpu=33EP512MU810',
    '-mdfp=' + device_pack_path,
    '-mlarge-code',
  ]
  linker_script = device_pack_path / 'support/dsPIC33E/gld/p33EP512MU810.gld'

  firmware_objects = []
  foreach name, source : {
    'analog-axis': 'source/analog/axis.c',
    'analog-samples': 'source/analog/samples.c',
    'cooling-controller': 'source/cooling/controller.c',
    'cooling-effect-limit': 'source/cooling/effect_limit.c',
    'cooling-pwm': 'source/cooling/pwm.c',
    'cooling-tachometer': 'source/cooling/tachometer.c',
    'cooling-temperature': 'source/cooling/temperature.c',
    'force-effect-bank': 'source/force_feedback/bank.c',
    'force-controller': 'source/force_feedback/controller.c',
    'force-effect': 'source/force_feedback/effect.c',
    'force-output': 'source/force_feedback/output.c',
    'force-output-report': 'source/force_feedback/output_report.c',
    'force-script-control': 'source/force_feedback/script_control.c',
    'force-filter': 'source/force_feedback/filter.c',
    'force-safety': 'source/force_feedback/safety.c',
    'force-soft-stop': 'source/force_feedback/soft_stop.c',
    'fanatec-input': 'source/usb/fanatec_input.c',
    'usb-descriptor': 'source/usb/descriptor.c',
    'usb-control-request': 'source/usb/control_request.c',
    'usb-control-pipe': 'source/usb/control_pipe.c',
    'usb-buffer-descriptor': 'source/usb/buffer_descriptor.c',
    'usb-device-control': 'source/usb/device_control.c',
    'usb-device': 'source/usb/device.c',
    'usb-output-command': 'source/usb/output_command.c',
    'usb-operating-mode-command': 'source/usb/operating_mode_command.c',
    'usb-vendor-command': 'source/usb/vendor_command.c',
    'podium-report-descriptor': 'source/usb/podium_report_descriptor.c',
    'board-identity': 'source/board/identity.c',
    'power-button': 'source/board/power_button.c',
    'status-led': 'source/board/status_led.c',
    'firmware': 'source/firmware.c',
    'h-pattern-shifter': 'source/shifter/h_pattern.c',
    'shifter-display': 'source/shifter/display.c',
    'logitech-input': 'source/usb/logitech_input.c',
    'motor-identity': 'source/motor/identity.c',
    'motor-live-frame': 'source/motor/live_frame.c',
    'motor-output-interlock': 'source/motor/output_interlock.c',
    'motor-probe': 'source/motor/probe.c',
    'motor-telemetry': 'source/motor/telemetry.c',
    'motor-telemetry-service': 'source/motor/telemetry_service.c',
    'motor-tuning-parameter': 'source/motor/tuning_parameter.c',
    'motor-tuning-service': 'source/motor/tuning_service.c',
    'motor-tuning-sync': 'source/motor/tuning_sync.c',
    'pedal-frame': 'source/pedal/frame.c',
    'pedal-axis': 'source/pedal/axis.c',
    'pedal-analog': 'source/pedal/analog.c',
    'pedal-input': 'source/pedal/input.c',
    'pedal-protocol': 'source/pedal/protocol.c',
    'pedal-service': 'source/pedal/service.c',
    'pedal-v4-status': 'source/pedal/v4_status.c',
    'pedal-v4-tuning': 'source/pedal/v4_tuning.c',
    'transfer-frame': 'source/transfer/frame.c',
    'transfer-session': 'source/transfer/session.c',
    'platform-clock': 'source/platform/dspic33e/clock.c',
    'platform-cooling': 'source/platform/dspic33e/cooling.c',
    'platform-board-identity': 'source/platform/dspic33e/board_identity.c',
    'platform-adc': 'source/platform/dspic33e/adc.c',
    'platform-aux-bus': 'source/platform/dspic33e/aux_bus.c',
    'platform-motor-link': 'source/platform/dspic33e/motor_link.c',
    'platform-pedal-link': 'source/platform/dspic33e/pedal_link.c',
    'platform-pin-mux': 'source/platform/dspic33e/pin_mux.c',
    'platform-storage': 'source/platform/dspic33e/storage.c',
    'platform-status-led': 'source/platform/dspic33e/status_led.c',
    'platform-shifter': 'source/platform/dspic33e/shifter.c',
    'platform-time': 'source/platform/dspic33e/time.c',
    'platform-usb': 'source/platform/dspic33e/usb.c',
    'platform-wheel-link': 'source/platform/dspic33e/wheel_link.c',
    'platform-time-common': 'source/platform/time.c',
    'profile-bank': 'source/profile/bank.c',
    'profile-record': 'source/profile/record.c',
    'profile-tuning': 'source/profile/tuning.c',
    'settings-persistence': 'source/settings/persistence.c',
    'settings-state': 'source/settings/state.c',
    'wheel-position': 'source/wheel/position.c',
    'wheel-axis-override': 'source/wheel/axis_override.c',
    'wheel-authentication': 'source/wheel/authentication.c',
    'wheel-display-output': 'source/wheel/display_output.c',
    'wheel-packet-mode-one': 'source/wheel/packet_mode_one.c',
    'wheel-protocol': 'source/wheel/protocol.c',
    'wheel-service': 'source/wheel/service.c',
    'wheel-transport-frame': 'source/wheel/transport_frame.c',
    'wheel-transport-service': 'source/wheel/transport_service.c',
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

  cooling_tachometer_test = executable(
    'base-cooling-tachometer-test',
    'tests/source/cooling_tachometer.c',
    cooling_tachometer_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  cooling_controller_test = executable(
    'base-cooling-controller-test',
    'tests/source/cooling_controller.c',
    cooling_controller_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  cooling_effect_limit_test = executable(
    'base-cooling-effect-limit-test',
    'tests/source/cooling_effect_limit.c',
    cooling_effect_limit_sources,
    cooling_controller_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  cooling_pwm_test = executable(
    'base-cooling-pwm-test',
    'tests/source/cooling_pwm.c',
    cooling_pwm_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  cooling_temperature_test = executable(
    'base-cooling-temperature-test',
    'tests/source/cooling_temperature.c',
    cooling_temperature_sources,
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

  force_controller_test = executable(
    'base-force-controller-test',
    'tests/source/force_controller.c',
    force_controller_sources,
    force_effect_bank_sources,
    force_effect_sources,
    force_filter_sources,
    force_output_sources,
    force_soft_stop_sources,
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

  force_safety_test = executable(
    'base-force-safety-test',
    'tests/source/force_safety.c',
    force_safety_sources,
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

  force_script_control_test = executable(
    'base-force-script-control-test',
    'tests/source/force_script_control.c',
    force_script_control_sources,
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

  usb_descriptor_test = executable(
    'base-usb-descriptor-test',
    'tests/source/usb_descriptor.c',
    usb_descriptor_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_control_request_test = executable(
    'base-usb-control-request-test',
    'tests/source/usb_control_request.c',
    usb_control_request_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_control_pipe_test = executable(
    'base-usb-control-pipe-test',
    'tests/source/usb_control_pipe.c',
    usb_control_pipe_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_buffer_descriptor_test = executable(
    'base-usb-buffer-descriptor-test',
    'tests/source/usb_buffer_descriptor.c',
    usb_buffer_descriptor_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_device_control_test = executable(
    'base-usb-device-control-test',
    'tests/source/usb_device_control.c',
    usb_device_control_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_device_test = executable(
    'base-usb-device-test',
    'tests/source/usb_device.c',
    usb_device_sources,
    usb_control_pipe_sources,
    usb_control_request_sources,
    usb_descriptor_sources,
    usb_device_control_sources,
    podium_report_descriptor_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_output_command_test = executable(
    'base-usb-output-command-test',
    'tests/source/usb_output_command.c',
    usb_output_command_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_operating_mode_command_test = executable(
    'base-usb-operating-mode-command-test',
    'tests/source/usb_operating_mode_command.c',
    usb_operating_mode_command_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_vendor_command_test = executable(
    'base-usb-vendor-command-test',
    'tests/source/usb_vendor_command.c',
    usb_vendor_command_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  podium_report_descriptor_test = executable(
    'base-podium-report-descriptor-test',
    'tests/source/podium_report_descriptor.c',
    podium_report_descriptor_sources,
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

  power_button_test = executable(
    'base-power-button-test',
    'tests/source/power_button.c',
    power_button_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  status_led_test = executable(
    'base-status-led-test',
    'tests/source/status_led.c',
    status_led_sources,
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

  shifter_display_test = executable(
    'base-shifter-display-test',
    'tests/source/shifter_display.c',
    shifter_display_sources,
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

  motor_identity_test = executable(
    'base-motor-identity-test',
    'tests/source/motor_identity.c',
    motor_identity_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_live_frame_test = executable(
    'base-motor-live-frame-test',
    'tests/source/motor_live_frame.c',
    motor_live_frame_sources,
    force_output_report_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_output_interlock_test = executable(
    'base-motor-output-interlock-test',
    'tests/source/motor_output_interlock.c',
    motor_output_interlock_sources,
    motor_identity_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_probe_test = executable(
    'base-motor-probe-test',
    'tests/source/motor_probe.c',
    motor_probe_sources,
    motor_identity_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_telemetry_service_test = executable(
    'base-motor-telemetry-service-test',
    'tests/source/motor_telemetry_service.c',
    motor_telemetry_service_sources,
    motor_telemetry_sources,
    motor_identity_sources,
    platform_time_sources,
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

  transfer_frame_test = executable(
    'base-transfer-frame-test',
    'tests/source/transfer_frame.c',
    transfer_frame_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  transfer_session_test = executable(
    'base-transfer-session-test',
    'tests/source/transfer_session.c',
    transfer_frame_sources,
    transfer_session_sources,
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

  pedal_input_test = executable(
    'base-pedal-input-test',
    'tests/source/pedal_input.c',
    pedal_input_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  pedal_analog_test = executable(
    'base-pedal-analog-test',
    'tests/source/pedal_analog.c',
    pedal_analog_sources,
    pedal_axis_sources,
    pedal_input_sources,
    analog_axis_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  pedal_protocol_test = executable(
    'base-pedal-protocol-test',
    'tests/source/pedal_protocol.c',
    pedal_protocol_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  pedal_service_test = executable(
    'base-pedal-service-test',
    'tests/source/pedal_service.c',
    pedal_service_sources,
    pedal_v4_status_sources,
    pedal_v4_tuning_sources,
    pedal_analog_sources,
    pedal_axis_sources,
    pedal_input_sources,
    pedal_frame_sources,
    pedal_protocol_sources,
    transfer_frame_sources,
    transfer_session_sources,
    analog_axis_sources,
    platform_time_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  pedal_v4_status_test = executable(
    'base-pedal-v4-status-test',
    'tests/source/pedal_v4_status.c',
    pedal_v4_status_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  pedal_v4_tuning_test = executable(
    'base-pedal-v4-tuning-test',
    'tests/source/pedal_v4_tuning.c',
    pedal_v4_tuning_sources,
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

  settings_persistence_test = executable(
    'base-settings-persistence-test',
    'tests/source/base_settings_persistence.c',
    settings_persistence_sources,
    settings_state_sources,
    profile_record_sources,
    profile_bank_sources,
    profile_tuning_sources,
    wheel_position_sources,
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

  wheel_position_test = executable(
    'base-wheel-position-test',
    'tests/source/wheel_position.c',
    wheel_position_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_axis_override_test = executable(
    'base-wheel-axis-override-test',
    'tests/source/wheel_axis_override.c',
    wheel_axis_override_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_display_output_test = executable(
    'base-wheel-display-output-test',
    'tests/source/wheel_display_output.c',
    wheel_display_output_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_transport_frame_test = executable(
    'base-wheel-transport-frame-test',
    'tests/source/wheel_transport_frame.c',
    wheel_transport_frame_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_packet_mode_one_test = executable(
    'base-wheel-packet-mode-one-test',
    'tests/source/wheel_packet_mode_one.c',
    wheel_packet_mode_one_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_protocol_test = executable(
    'base-wheel-protocol-test',
    'tests/source/wheel_protocol.c',
    wheel_protocol_sources,
    wheel_authentication_sources,
    wheel_axis_override_sources,
    wheel_packet_mode_one_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_service_test = executable(
    'base-wheel-service-test',
    'tests/source/wheel_service.c',
    wheel_service_sources,
    wheel_display_output_sources,
    wheel_authentication_sources,
    wheel_axis_override_sources,
    wheel_packet_mode_one_sources,
    wheel_protocol_sources,
    wheel_transport_service_sources,
    wheel_transport_frame_sources,
    platform_time_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_transport_service_test = executable(
    'base-wheel-transport-service-test',
    'tests/source/wheel_transport_service.c',
    wheel_transport_service_sources,
    wheel_transport_frame_sources,
    platform_time_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  test('base-analog-axis', analog_axis_test, suite: ['base', 'analog'])
  test(
    'base-cooling-controller',
    cooling_controller_test,
    suite: ['base', 'cooling'],
  )
  test(
    'base-cooling-effect-limit',
    cooling_effect_limit_test,
    suite: ['base', 'cooling', 'force-feedback'],
  )
  test(
    'base-cooling-tachometer',
    cooling_tachometer_test,
    suite: ['base', 'cooling'],
  )
  test(
    'base-cooling-pwm',
    cooling_pwm_test,
    suite: ['base', 'cooling'],
  )
  test(
    'base-cooling-temperature',
    cooling_temperature_test,
    suite: ['base', 'cooling'],
  )
  test('base-analog-samples', analog_samples_test, suite: ['base', 'analog'])
  test('base-force-effect', force_effect_test, suite: ['base', 'force-feedback'])
  test('base-force-effect-bank', force_effect_bank_test, suite: ['base', 'force-feedback'])
  test('base-force-controller', force_controller_test, suite: ['base', 'force-feedback'])
  test('base-force-output', force_output_test, suite: ['base', 'force-feedback'])
  test(
    'base-force-output-report',
    force_output_report_test,
    suite: ['base', 'force-feedback', 'transport'],
  )
  test(
    'base-force-script-control',
    force_script_control_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test('base-force-filter', force_filter_test, suite: ['base', 'force-feedback'])
  test('base-force-safety', force_safety_test, suite: ['base', 'force-feedback'])
  test('base-force-soft-stop', force_soft_stop_test, suite: ['base', 'force-feedback'])
  test('base-fanatec-input', fanatec_input_test, suite: ['base', 'usb', 'input'])
  test('base-usb-descriptor', usb_descriptor_test, suite: ['base', 'usb'])
  test('base-usb-control-request', usb_control_request_test, suite: ['base', 'usb'])
  test('base-usb-control-pipe', usb_control_pipe_test, suite: ['base', 'usb'])
  test('base-usb-buffer-descriptor', usb_buffer_descriptor_test, suite: ['base', 'usb'])
  test('base-usb-device-control', usb_device_control_test, suite: ['base', 'usb'])
  test('base-usb-device', usb_device_test, suite: ['base', 'usb'])
  test('base-usb-output-command', usb_output_command_test, suite: ['base', 'usb', 'output'])
  test(
    'base-usb-operating-mode-command',
    usb_operating_mode_command_test,
    suite: ['base', 'usb', 'output'],
  )
  test('base-usb-vendor-command', usb_vendor_command_test, suite: ['base', 'usb', 'output'])
  test(
    'base-podium-report-descriptor',
    podium_report_descriptor_test,
    suite: ['base', 'usb'],
  )
  test('base-board-identity', board_identity_test, suite: ['base', 'board'])
  test('base-power-button', power_button_test, suite: ['base', 'board', 'input'])
  test('base-status-led', status_led_test, suite: ['base', 'board', 'led'])
  test('base-h-pattern-shifter', h_pattern_shifter_test, suite: ['base', 'shifter'])
  test('base-shifter-display', shifter_display_test, suite: ['base', 'shifter', 'display'])
  test('base-logitech-input', logitech_input_test, suite: ['base', 'usb', 'input'])
  test('base-motor-identity', motor_identity_test, suite: ['base', 'motor'])
  test('base-motor-live-frame', motor_live_frame_test, suite: ['base', 'motor', 'transport'])
  test('base-motor-output-interlock', motor_output_interlock_test, suite: ['base', 'motor'])
  test('base-motor-probe', motor_probe_test, suite: ['base', 'motor'])
  test('base-motor-telemetry-service', motor_telemetry_service_test, suite: ['base', 'motor'])
  test('base-motor-tuning-parameter', motor_tuning_parameter_test, suite: ['base', 'motor'])
  test('base-motor-tuning-sync', motor_tuning_sync_test, suite: ['base', 'motor'])
  test('base-motor-tuning-service', motor_tuning_service_test, suite: ['base', 'motor'])
  test('base-pedal-frame', pedal_frame_test, suite: ['base', 'pedal', 'transport'])
  test('base-transfer-frame', transfer_frame_test, suite: ['base', 'transfer'])
  test('base-transfer-session', transfer_session_test, suite: ['base', 'transfer'])
  test('base-pedal-axis', pedal_axis_test, suite: ['base', 'pedal', 'analog'])
  test('base-pedal-input', pedal_input_test, suite: ['base', 'pedal', 'input'])
  test('base-pedal-analog', pedal_analog_test, suite: ['base', 'pedal', 'analog'])
  test('base-pedal-protocol', pedal_protocol_test, suite: ['base', 'pedal', 'transport'])
  test('base-pedal-service', pedal_service_test, suite: ['base', 'pedal'])
  test('base-pedal-v4-status', pedal_v4_status_test, suite: ['base', 'pedal'])
  test('base-pedal-v4-tuning', pedal_v4_tuning_test, suite: ['base', 'pedal'])
  test('base-profile-tuning', profile_tuning_test, suite: ['base', 'profile'])
  test('base-profile-bank', profile_bank_test, suite: ['base', 'profile'])
  test('base-profile-record', profile_record_test, suite: ['base', 'profile', 'persistence'])
  test(
    'base-settings-persistence',
    settings_persistence_test,
    suite: ['base', 'settings', 'persistence'],
  )
  test('base-platform-time', platform_time_test, suite: ['base', 'platform'])
  test('base-wheel-position', wheel_position_test, suite: ['base', 'wheel-position'])
  test('base-wheel-axis-override', wheel_axis_override_test, suite: ['base', 'wheel', 'input'])
  test(
    'base-wheel-display-output',
    wheel_display_output_test,
    suite: ['base', 'wheel', 'display'],
  )
  test('base-wheel-protocol', wheel_protocol_test, suite: ['base', 'wheel', 'protocol'])
  test(
    'base-wheel-packet-mode-one',
    wheel_packet_mode_one_test,
    suite: ['base', 'wheel', 'protocol'],
  )
  test('base-wheel-service', wheel_service_test, suite: ['base', 'wheel', 'input'])
  test(
    'base-wheel-transport-frame',
    wheel_transport_frame_test,
    suite: ['base', 'wheel', 'transport'],
  )
  test(
    'base-wheel-transport-service',
    wheel_transport_service_test,
    suite: ['base', 'wheel', 'transport'],
  )
endif
