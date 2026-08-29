base_include = include_directories('source')
base_source_path = meson.current_source_dir() / 'source'
common_source_path = meson.project_source_root() / 'source'
analog_axis_sources = files('source/analog/axis.c')
auxiliary_axis_sources = files('source/analog/auxiliary_axis.c')
analog_samples_sources = files('source/analog/samples.c')
cooling_controller_sources = files('source/cooling/controller.c')
cooling_effect_limit_sources = files('source/cooling/effect_limit.c')
cooling_pwm_sources = files('source/cooling/pwm.c')
cooling_tachometer_sources = files('source/cooling/tachometer.c')
cooling_temperature_sources = files('source/cooling/temperature.c')
display_controller_sources = files('source/display/controller.c')
display_framebuffer_sources = files('source/display/framebuffer.c')
display_notice_sources = files('source/display/notice.c')
display_prompt_sources = files('source/display/prompt.c')
display_text_sources = files('source/display/text.c')
force_effect_bank_sources = files('source/force_feedback/bank.c')
force_feedback_command_sources = files('source/force_feedback/command.c')
force_feedback_state_sources = files('source/force_feedback/state.c')
force_controller_sources = files('source/force_feedback/controller.c')
force_effect_sources = files('source/force_feedback/effect.c')
force_output_sources = files('source/force_feedback/output.c')
force_output_enable_sources = files('source/force_feedback/output_enable.c')
force_output_report_sources = files('source/force_feedback/output_report.c')
force_output_scale_sources = files('source/force_feedback/output_scale.c')
force_script_bits_sources = files('source/force_feedback/script_bits.c')
force_script_compare_sources = files('source/force_feedback/script_compare.c')
force_script_control_sources = files('source/force_feedback/script_control.c')
force_script_executor_sources = files('source/force_feedback/script_executor.c')
force_script_input_sources = files('source/force_feedback/script_input.c')
force_script_integer_sources = files('source/force_feedback/script_integer.c')
force_script_logic_sources = files('source/force_feedback/script_logic.c')
force_script_math_sources = files('source/force_feedback/script_math.c')
force_script_motion_sources = files('source/force_feedback/script_motion.c')
force_script_operand_sources = files('source/force_feedback/script_operand.c')
force_script_operation_sources = files('source/force_feedback/script_operation.c')
force_script_output_sources = files('source/force_feedback/script_output.c')
force_script_range_sources = files('source/force_feedback/script_range.c')
force_script_report_sources = files('source/force_feedback/script_report.c')
force_script_runtime_sources = files('source/force_feedback/script_runtime.c')
force_script_sample_sources = files('source/force_feedback/script_sample.c')
force_script_scheduler_sources = files('source/force_feedback/script_scheduler.c')
force_script_service_sources = files('source/force_feedback/script_service.c')
force_script_store_sources = files('source/force_feedback/script_store.c')
force_script_tick_sources = files('source/force_feedback/script_tick.c')
force_filter_sources = files('source/force_feedback/filter.c')
force_safety_sources = files('source/force_feedback/safety.c')
force_soft_stop_sources = files('source/force_feedback/soft_stop.c')
fanatec_input_sources = files('source/usb/fanatec_input.c')
fanatec_encoder_sources = files('source/usb/fanatec_encoder.c')
usb_input_report_sources = files('source/usb/input_report.c')
usb_compatibility_report_descriptor_sources = files(
  'source/usb/compatibility_report_descriptor.c',
)
usb_compatibility_descriptor_sources = files('source/usb/compatibility_descriptor.c')
usb_console_descriptor_sources = files('source/usb/console_descriptor.c')
usb_feature_download_sources = files('source/usb/feature_download.c')
usb_feature_upload_sources = files('source/usb/feature_upload.c')
usb_feature_upload_acknowledgement_sources = files(
  'source/usb/feature_upload_acknowledgement.c',
)
usb_motor_command_upload_sources = files('source/usb/motor_command_upload.c')
usb_motor_response_download_sources = files('source/usb/motor_response_download.c')
usb_motor_vendor_service_sources = files('source/usb/motor_vendor_service.c')
usb_xbox_gip_command_sources = files('source/usb/xbox_gip_command.c')
usb_xbox_gip_discovery_sources = files('source/usb/xbox_gip_discovery.c')
usb_xbox_gip_metadata_sources = files('source/usb/xbox_gip_metadata.c')
usb_xbox_gip_metadata_download_sources = files('source/usb/xbox_gip_metadata_download.c')
usb_xbox_gip_input_sources = files('source/usb/xbox_gip_input.c')
usb_xbox_gip_response_sources = files('source/usb/xbox_gip_response.c')
usb_xbox_gip_service_sources = files('source/usb/xbox_gip_service.c')
usb_xbox_gip_session_sources = files('source/usb/xbox_gip_session.c')
usb_xbox_gip_vendor_tunnel_sources = files('source/usb/xbox_gip_vendor_tunnel.c')
usb_updater_descriptor_sources = files('source/usb/updater_descriptor.c')
usb_updater_control_sources = files('source/usb/updater_control.c')
usb_descriptor_sources = files('source/usb/descriptor.c')
usb_diagnostic_report_sources = files('source/usb/diagnostic_report.c')
usb_tuning_menu_service_sources = files('source/usb/tuning_menu_service.c')
usb_tuning_profile_report_sources = files('source/usb/tuning_profile_report.c')
usb_tuning_profile_service_sources = files('source/usb/tuning_profile_service.c')
usb_control_request_sources = files('source/usb/control_request.c')
usb_control_pipe_sources = files('source/usb/control_pipe.c')
usb_buffer_descriptor_sources = files('source/usb/buffer_descriptor.c')
usb_device_control_sources = files('source/usb/device_control.c')
usb_device_sources = files('source/usb/device.c')
usb_output_command_sources = files('source/usb/output_command.c')
usb_operating_mode_command_sources = files('source/usb/operating_mode_command.c')
usb_remote_tuning_records_sources = files('source/usb/remote_tuning_records.c')
usb_remote_tuning_service_sources = files('source/usb/remote_tuning_service.c')
remote_tuning_telemetry_sources = files('source/remote_tuning/telemetry.c')
usb_vendor_command_sources = files('source/usb/vendor_command.c')
podium_report_descriptor_sources = files('source/usb/podium_report_descriptor.c')
board_identity_sources = files('source/board/identity.c')
usb_connection_sources = files('source/usb/connection.c')
usb_host_capability_recovery_sources = files('source/usb/host_capability_recovery.c')
i2c_probe_sources = files('source/i2c/probe.c')
i2c_probe_bus_sources = files('source/i2c/probe_bus.c')
i2c_probe_exchange_service_sources = files('source/i2c/probe_exchange_service.c')
i2c_probe_packet_sources = files('source/i2c/probe_packet.c')
i2c_probe_startup_service_sources = files('source/i2c/probe_startup_service.c')
i2c_probe_transfer_service_sources = files('source/i2c/probe_transfer_service.c')
status_led_sources = files('source/board/status_led.c')
power_controller_sources = files('source/board/power.c')
torque_key_sources = files('source/board/torque_key.c')
system_torque_transition_sources = files('source/system/torque_transition.c')
torque_key_prompt_sources = files('source/system/torque_key_prompt.c')
system_event_queue_sources = files('source/system/event_queue.c')
system_event_dispatcher_sources = files('source/system/event_dispatcher.c')
system_notice_sources = files('source/system/notice.c')
system_control_state_sources = files('source/system/control_state.c')
h_pattern_shifter_sources = files('source/shifter/h_pattern.c')
h_pattern_calibration_sources = files('source/shifter/calibration.c')
shifter_display_sources = files('source/shifter/display.c')
logitech_input_sources = files('source/usb/logitech_input.c')
motor_identity_sources = files('source/motor/identity.c')
motor_live_frame_sources = files('source/motor/live_frame.c')
motor_output_transport_sources = files('source/motor/output_transport.c')
motor_output_interlock_sources = files('source/motor/output_interlock.c')
motor_probe_sources = files('source/motor/probe.c')
motor_calibration_sources = files('source/motor/calibration.c')
motor_status_service_sources = files('source/motor/status_service.c')
motor_telemetry_sources = files('source/motor/telemetry.c')
motor_telemetry_service_sources = files('source/motor/telemetry_service.c')
motor_tuning_parameter_sources = files('source/motor/tuning_parameter.c')
motor_tuning_service_sources = files('source/motor/tuning_service.c')
motor_tuning_sync_sources = files('source/motor/tuning_sync.c')
pedal_frame_sources = files('source/pedal/frame.c')
pedal_axis_sources = files('source/pedal/axis.c')
pedal_analog_sources = files('source/pedal/analog.c')
pedal_brake_indicator_sources = files('source/pedal/brake_indicator.c')
pedal_calibration_command_sources = files('source/pedal/calibration_command.c')
pedal_input_sources = files('source/pedal/input.c')
pedal_protocol_sources = files('source/pedal/protocol.c')
pedal_protocol_command_sources = files('source/pedal/protocol_command.c')
pedal_service_sources = files('source/pedal/service.c')
pedal_v4_status_sources = files('source/pedal/v4_status.c')
pedal_v4_tuning_sources = files('source/pedal/v4_tuning.c')
transfer_frame_sources = files('source/transfer/frame.c')
transfer_session_sources = files('source/transfer/session.c')
transfer_command_sources = files('source/transfer/command.c')
profile_bank_sources = files('source/profile/bank.c')
profile_record_sources = files('source/profile/record.c')
profile_tuning_sources = files('source/profile/tuning.c')
settings_persistence_sources = files('source/settings/persistence.c')
settings_state_sources = files('source/settings/state.c')
platform_time_sources = files('source/platform/time.c')
wheel_position_sources = files('source/wheel/position.c')
wheel_center_capture_sources = files('source/wheel/center_capture.c')
wheel_steering_limit_sources = files('source/wheel/steering_limit.c')
wheel_axis_override_sources = files('source/wheel/axis_override.c')
wheel_authentication_sources = files('source/wheel/authentication.c')
wheel_capability_sources = files('source/wheel/capability.c')
wheel_command_forwarder_sources = files('source/wheel/command_forwarder.c')
wheel_compatibility_alert_sources = files('source/wheel/compatibility_alert.c')
wheel_display_output_sources = files('source/wheel/display_output.c')
wheel_usb_disconnect_display_sources = files('source/wheel/usb_disconnect_display.c')
wheel_output_reports_sources = files('source/wheel/output_reports.c')
transfer_memory_sources = files('source/transfer/memory.c')
wheel_motion_sources = files('source/wheel/motion.c')
wheel_rotary_input_sources = files('source/wheel/rotary_input.c')
wheel_packet_crc_sources = files('source/wheel/packet_crc.c')
wheel_packet_mode_one_sources = files('source/wheel/packet_mode_one.c')
wheel_packet_mode_four_sources = files('source/wheel/packet_mode_four.c')
wheel_packet_remote_tuning_sources = files('source/wheel/packet_remote_tuning.c')
wheel_protocol_sources = files('source/wheel/protocol.c')
wheel_service_sources = files('source/wheel/service.c')
wheel_status_service_sources = files('source/wheel/status_service.c')
wheel_transfer_service_sources = files('source/wheel/transfer_service.c')
wheel_vibration_sources = files('source/wheel/vibration.c')
motor_command_digest_sources = files('source/motor/command_digest.c')
motor_command_application_sources = files('source/motor/command_application.c')
motor_command_channel_sources = files('source/motor/command_channel.c')
motor_command_channel_mailbox_sources = files('source/motor/command_channel_mailbox.c')
motor_command_fragment_sources = files('source/motor/command_fragment.c')
motor_command_information_sources = files('source/motor/command_information.c')
motor_command_mailbox_sources = files('source/motor/command_mailbox.c')
motor_command_message_sources = files('source/motor/command_message.c')
motor_command_packet_sources = files('source/motor/command_packet.c')
motor_command_receiver_sources = files('source/motor/command_receiver.c')
motor_command_scheduler_sources = files('source/motor/command_scheduler.c')
motor_command_sequence_sources = files('source/motor/command_sequence.c')
motor_command_serial_sources = files('source/motor/command_serial.c')
motor_command_startup_sources = files('source/motor/command_startup.c')
motor_command_startup_service_sources = files('source/motor/command_startup_service.c')
serial_message_sources = files('source/serial/message.c')
serial_packet_sources = files('source/serial/packet.c')
serial_service_sources = files('source/serial/service.c')
serial_session_sources = files('source/serial/session.c')

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
    'auxiliary-axis': 'source/analog/auxiliary_axis.c',
    'analog-samples': 'source/analog/samples.c',
    'cooling-controller': 'source/cooling/controller.c',
    'cooling-effect-limit': 'source/cooling/effect_limit.c',
    'cooling-pwm': 'source/cooling/pwm.c',
    'cooling-tachometer': 'source/cooling/tachometer.c',
    'cooling-temperature': 'source/cooling/temperature.c',
    'display-controller': 'source/display/controller.c',
    'display-framebuffer': 'source/display/framebuffer.c',
    'display-notice': 'source/display/notice.c',
    'display-prompt': 'source/display/prompt.c',
    'display-text': 'source/display/text.c',
    'force-effect-bank': 'source/force_feedback/bank.c',
    'force-feedback-command': 'source/force_feedback/command.c',
    'force-feedback-state': 'source/force_feedback/state.c',
    'force-controller': 'source/force_feedback/controller.c',
    'force-effect': 'source/force_feedback/effect.c',
    'force-output': 'source/force_feedback/output.c',
    'force-output-enable': 'source/force_feedback/output_enable.c',
    'force-output-report': 'source/force_feedback/output_report.c',
    'force-output-scale': 'source/force_feedback/output_scale.c',
    'force-script-bits': 'source/force_feedback/script_bits.c',
    'force-script-compare': 'source/force_feedback/script_compare.c',
    'force-script-control': 'source/force_feedback/script_control.c',
    'force-script-executor': 'source/force_feedback/script_executor.c',
    'force-script-input': 'source/force_feedback/script_input.c',
    'force-script-integer': 'source/force_feedback/script_integer.c',
    'force-script-logic': 'source/force_feedback/script_logic.c',
    'force-script-math': 'source/force_feedback/script_math.c',
    'force-script-motion': 'source/force_feedback/script_motion.c',
    'force-script-operand': 'source/force_feedback/script_operand.c',
    'force-script-operation': 'source/force_feedback/script_operation.c',
    'force-script-output': 'source/force_feedback/script_output.c',
    'force-script-range': 'source/force_feedback/script_range.c',
    'force-script-report': 'source/force_feedback/script_report.c',
    'force-script-runtime': 'source/force_feedback/script_runtime.c',
    'force-script-sample': 'source/force_feedback/script_sample.c',
    'force-script-scheduler': 'source/force_feedback/script_scheduler.c',
    'force-script-service': 'source/force_feedback/script_service.c',
    'force-script-store': 'source/force_feedback/script_store.c',
    'force-script-tick': 'source/force_feedback/script_tick.c',
    'force-filter': 'source/force_feedback/filter.c',
    'force-safety': 'source/force_feedback/safety.c',
    'force-soft-stop': 'source/force_feedback/soft_stop.c',
    'fanatec-input': 'source/usb/fanatec_input.c',
    'fanatec-encoder': 'source/usb/fanatec_encoder.c',
    'usb-input-report': 'source/usb/input_report.c',
    'usb-compatibility-report-descriptor': 'source/usb/compatibility_report_descriptor.c',
    'usb-compatibility-descriptor': 'source/usb/compatibility_descriptor.c',
    'usb-console-descriptor': 'source/usb/console_descriptor.c',
    'usb-feature-download': 'source/usb/feature_download.c',
    'usb-feature-upload': 'source/usb/feature_upload.c',
    'usb-feature-upload-acknowledgement': 'source/usb/feature_upload_acknowledgement.c',
    'usb-motor-command-upload': 'source/usb/motor_command_upload.c',
    'usb-motor-response-download': 'source/usb/motor_response_download.c',
    'usb-motor-vendor-service': 'source/usb/motor_vendor_service.c',
    'usb-xbox-gip-command': 'source/usb/xbox_gip_command.c',
    'usb-xbox-gip-discovery': 'source/usb/xbox_gip_discovery.c',
    'usb-xbox-gip-metadata': 'source/usb/xbox_gip_metadata.c',
    'usb-xbox-gip-metadata-download': 'source/usb/xbox_gip_metadata_download.c',
    'usb-xbox-gip-input': 'source/usb/xbox_gip_input.c',
    'usb-xbox-gip-response': 'source/usb/xbox_gip_response.c',
    'usb-xbox-gip-service': 'source/usb/xbox_gip_service.c',
    'usb-xbox-gip-session': 'source/usb/xbox_gip_session.c',
    'usb-xbox-gip-vendor-tunnel': 'source/usb/xbox_gip_vendor_tunnel.c',
    'usb-updater-descriptor': 'source/usb/updater_descriptor.c',
    'usb-updater-control': 'source/usb/updater_control.c',
    'usb-descriptor': 'source/usb/descriptor.c',
    'usb-diagnostic-report': 'source/usb/diagnostic_report.c',
    'usb-tuning-menu-service': 'source/usb/tuning_menu_service.c',
    'usb-tuning-profile-report': 'source/usb/tuning_profile_report.c',
    'usb-tuning-profile-service': 'source/usb/tuning_profile_service.c',
    'usb-control-request': 'source/usb/control_request.c',
    'usb-control-pipe': 'source/usb/control_pipe.c',
    'usb-buffer-descriptor': 'source/usb/buffer_descriptor.c',
    'usb-device-control': 'source/usb/device_control.c',
    'usb-device': 'source/usb/device.c',
    'usb-output-command': 'source/usb/output_command.c',
    'usb-operating-mode-command': 'source/usb/operating_mode_command.c',
    'usb-remote-tuning-records': 'source/usb/remote_tuning_records.c',
    'usb-remote-tuning-service': 'source/usb/remote_tuning_service.c',
    'remote-tuning-telemetry': 'source/remote_tuning/telemetry.c',
    'usb-vendor-command': 'source/usb/vendor_command.c',
    'podium-report-descriptor': 'source/usb/podium_report_descriptor.c',
    'board-identity': 'source/board/identity.c',
    'usb-connection': 'source/usb/connection.c',
    'usb-host-capability-recovery': 'source/usb/host_capability_recovery.c',
    'i2c-probe': 'source/i2c/probe.c',
    'i2c-probe-bus': 'source/i2c/probe_bus.c',
    'i2c-probe-exchange-service': 'source/i2c/probe_exchange_service.c',
    'i2c-probe-packet': 'source/i2c/probe_packet.c',
    'i2c-probe-startup-service': 'source/i2c/probe_startup_service.c',
    'i2c-probe-transfer-service': 'source/i2c/probe_transfer_service.c',
    'status-led': 'source/board/status_led.c',
    'power-controller': 'source/board/power.c',
    'torque-key': 'source/board/torque_key.c',
    'system-torque-transition': 'source/system/torque_transition.c',
    'torque-key-prompt': 'source/system/torque_key_prompt.c',
    'system-event-queue': 'source/system/event_queue.c',
    'system-event-dispatcher': 'source/system/event_dispatcher.c',
    'system-notice': 'source/system/notice.c',
    'system-control-state': 'source/system/control_state.c',
    'firmware': 'source/firmware.c',
    'h-pattern-shifter': 'source/shifter/h_pattern.c',
    'h-pattern-calibration': 'source/shifter/calibration.c',
    'shifter-display': 'source/shifter/display.c',
    'logitech-input': 'source/usb/logitech_input.c',
    'motor-identity': 'source/motor/identity.c',
    'motor-live-frame': 'source/motor/live_frame.c',
    'motor-output-transport': 'source/motor/output_transport.c',
    'motor-output-interlock': 'source/motor/output_interlock.c',
    'motor-probe': 'source/motor/probe.c',
    'motor-calibration': 'source/motor/calibration.c',
    'motor-status-service': 'source/motor/status_service.c',
    'motor-telemetry': 'source/motor/telemetry.c',
    'motor-telemetry-service': 'source/motor/telemetry_service.c',
    'motor-tuning-parameter': 'source/motor/tuning_parameter.c',
    'motor-tuning-service': 'source/motor/tuning_service.c',
    'motor-tuning-sync': 'source/motor/tuning_sync.c',
    'pedal-frame': 'source/pedal/frame.c',
    'pedal-axis': 'source/pedal/axis.c',
    'pedal-analog': 'source/pedal/analog.c',
    'pedal-brake-indicator': 'source/pedal/brake_indicator.c',
    'pedal-calibration-command': 'source/pedal/calibration_command.c',
    'pedal-input': 'source/pedal/input.c',
    'pedal-protocol': 'source/pedal/protocol.c',
    'pedal-protocol-command': 'source/pedal/protocol_command.c',
    'pedal-service': 'source/pedal/service.c',
    'pedal-v4-status': 'source/pedal/v4_status.c',
    'pedal-v4-tuning': 'source/pedal/v4_tuning.c',
    'transfer-frame': 'source/transfer/frame.c',
    'transfer-session': 'source/transfer/session.c',
    'platform-clock': 'source/platform/dspic33e/clock.c',
    'platform-cooling': 'source/platform/dspic33e/cooling.c',
    'platform-display': 'source/platform/dspic33e/display.c',
    'platform-force-feedback-timer': 'source/platform/dspic33e/force_feedback_timer.c',
    'platform-board-identity': 'source/platform/dspic33e/board_identity.c',
    'platform-adc': 'source/platform/dspic33e/adc.c',
    'platform-aux-bus': 'source/platform/dspic33e/aux_bus.c',
    'platform-motor-link': 'source/platform/dspic33e/motor_link.c',
    'platform-pedal-link': 'source/platform/dspic33e/pedal_link.c',
    'platform-pin-mux': 'source/platform/dspic33e/pin_mux.c',
    'platform-storage': 'source/platform/dspic33e/storage.c',
    'platform-status-led': 'source/platform/dspic33e/status_led.c',
    'platform-power': 'source/platform/dspic33e/power.c',
    'platform-system': 'source/platform/dspic33e/system.c',
    'platform-torque-key': 'source/platform/dspic33e/torque_key.c',
    'platform-shifter': 'source/platform/dspic33e/shifter.c',
    'platform-time': 'source/platform/dspic33e/time.c',
    'platform-usb': 'source/platform/dspic33e/usb.c',
    'platform-serial-link': 'source/platform/dspic33e/serial_link.c',
    'platform-time-common': 'source/platform/time.c',
    'profile-bank': 'source/profile/bank.c',
    'profile-record': 'source/profile/record.c',
    'profile-tuning': 'source/profile/tuning.c',
    'settings-persistence': 'source/settings/persistence.c',
    'settings-state': 'source/settings/state.c',
    'wheel-position': 'source/wheel/position.c',
    'wheel-center-capture': 'source/wheel/center_capture.c',
    'wheel-steering-limit': 'source/wheel/steering_limit.c',
    'wheel-axis-override': 'source/wheel/axis_override.c',
    'wheel-authentication': 'source/wheel/authentication.c',
    'wheel-capability': 'source/wheel/capability.c',
    'wheel-command-forwarder': 'source/wheel/command_forwarder.c',
    'wheel-compatibility-alert': 'source/wheel/compatibility_alert.c',
    'wheel-display-output': 'source/wheel/display_output.c',
    'wheel-usb-disconnect-display': 'source/wheel/usb_disconnect_display.c',
    'wheel-output-reports': 'source/wheel/output_reports.c',
    'transfer-memory': 'source/transfer/memory.c',
    'transfer-command': 'source/transfer/command.c',
    'wheel-motion': 'source/wheel/motion.c',
    'wheel-rotary-input': 'source/wheel/rotary_input.c',
    'wheel-packet-crc': 'source/wheel/packet_crc.c',
    'wheel-packet-mode-one': 'source/wheel/packet_mode_one.c',
    'wheel-packet-mode-four': 'source/wheel/packet_mode_four.c',
    'wheel-packet-remote-tuning': 'source/wheel/packet_remote_tuning.c',
    'wheel-protocol': 'source/wheel/protocol.c',
    'wheel-service': 'source/wheel/service.c',
    'wheel-status-service': 'source/wheel/status_service.c',
    'wheel-transfer-service': 'source/wheel/transfer_service.c',
    'wheel-vibration': 'source/wheel/vibration.c',
    'motor-command-digest': 'source/motor/command_digest.c',
    'motor-command-application': 'source/motor/command_application.c',
    'motor-command-channel': 'source/motor/command_channel.c',
    'motor-command-channel-mailbox': 'source/motor/command_channel_mailbox.c',
    'motor-command-fragment': 'source/motor/command_fragment.c',
    'motor-command-information': 'source/motor/command_information.c',
    'motor-command-mailbox': 'source/motor/command_mailbox.c',
    'motor-command-message': 'source/motor/command_message.c',
    'motor-command-packet': 'source/motor/command_packet.c',
    'motor-command-receiver': 'source/motor/command_receiver.c',
    'motor-command-scheduler': 'source/motor/command_scheduler.c',
    'motor-command-sequence': 'source/motor/command_sequence.c',
    'motor-command-serial': 'source/motor/command_serial.c',
    'motor-command-startup': 'source/motor/command_startup.c',
    'motor-command-startup-service': 'source/motor/command_startup_service.c',
    'serial-message': 'source/serial/message.c',
    'serial-packet': 'source/serial/packet.c',
    'serial-service': 'source/serial/service.c',
    'serial-session': 'source/serial/session.c',
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

  auxiliary_axis_test = executable(
    'base-auxiliary-axis-test',
    'tests/source/auxiliary_axis.c',
    auxiliary_axis_sources,
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

  display_controller_test = executable(
    'base-display-controller-test',
    'tests/source/display_controller.c',
    display_controller_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  display_framebuffer_test = executable(
    'base-display-framebuffer-test',
    'tests/source/display_framebuffer.c',
    display_framebuffer_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  display_prompt_test = executable(
    'base-display-prompt-test',
    'tests/source/display_prompt.c',
    display_prompt_sources,
    display_text_sources,
    display_framebuffer_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  display_notice_test = executable(
    'base-display-notice-test',
    'tests/source/display_notice.c',
    display_notice_sources,
    display_text_sources,
    display_framebuffer_sources,
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

  force_feedback_command_test = executable(
    'base-force-feedback-command-test',
    'tests/source/force_feedback_command.c',
    force_feedback_command_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_feedback_state_test = executable(
    'base-force-feedback-state-test',
    'tests/source/force_feedback_state.c',
    force_feedback_state_sources,
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

  force_output_enable_test = executable(
    'base-force-output-enable-test',
    'tests/source/force_output_enable.c',
    force_output_enable_sources,
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

  force_output_scale_test = executable(
    'base-force-output-scale-test',
    'tests/source/force_output_scale.c',
    force_output_scale_sources,
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

  force_script_report_test = executable(
    'base-force-script-report-test',
    'tests/source/force_script_report.c',
    force_script_report_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_executor_test = executable(
    'base-force-script-executor-test',
    'tests/source/force_script_executor.c',
    force_script_executor_sources,
    force_script_operation_sources,
    force_script_operand_sources,
    force_script_bits_sources,
    force_script_compare_sources,
    force_script_integer_sources,
    force_script_logic_sources,
    force_script_math_sources,
    force_script_range_sources,
    force_script_sample_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_service_test = executable(
    'base-force-script-service-test',
    'tests/source/force_script_service.c',
    force_script_service_sources,
    force_script_executor_sources,
    force_script_operation_sources,
    force_script_operand_sources,
    force_script_store_sources,
    force_script_bits_sources,
    force_script_compare_sources,
    force_script_integer_sources,
    force_script_logic_sources,
    force_script_math_sources,
    force_script_range_sources,
    force_script_sample_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_scheduler_test = executable(
    'base-force-script-scheduler-test',
    'tests/source/force_script_scheduler.c',
    force_script_scheduler_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_tick_test = executable(
    'base-force-script-tick-test',
    'tests/source/force_script_tick.c',
    force_script_tick_sources,
    force_script_runtime_sources,
    force_script_control_sources,
    force_script_scheduler_sources,
    force_script_service_sources,
    force_script_executor_sources,
    force_script_operation_sources,
    force_script_operand_sources,
    force_script_motion_sources,
    force_script_store_sources,
    force_script_input_sources,
    force_script_bits_sources,
    force_script_compare_sources,
    force_script_integer_sources,
    force_script_logic_sources,
    force_script_math_sources,
    force_script_range_sources,
    force_script_sample_sources,
    force_script_output_sources,
    force_filter_sources,
    force_soft_stop_sources,
    force_output_scale_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_runtime_test = executable(
    'base-force-script-runtime-test',
    'tests/source/force_script_runtime.c',
    force_script_runtime_sources,
    force_script_control_sources,
    force_script_input_sources,
    force_script_store_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_bits_test = executable(
    'base-force-script-bits-test',
    'tests/source/force_script_bits.c',
    force_script_bits_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_compare_test = executable(
    'base-force-script-compare-test',
    'tests/source/force_script_compare.c',
    force_script_compare_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_input_test = executable(
    'base-force-script-input-test',
    'tests/source/force_script_input.c',
    force_script_input_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_integer_test = executable(
    'base-force-script-integer-test',
    'tests/source/force_script_integer.c',
    force_script_integer_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_math_test = executable(
    'base-force-script-math-test',
    'tests/source/force_script_math.c',
    force_script_math_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_motion_test = executable(
    'base-force-script-motion-test',
    'tests/source/force_script_motion.c',
    force_script_motion_sources,
    force_script_range_sources,
    force_script_input_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_operand_test = executable(
    'base-force-script-operand-test',
    'tests/source/force_script_operand.c',
    force_script_operand_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_operation_test = executable(
    'base-force-script-operation-test',
    'tests/source/force_script_operation.c',
    force_script_operation_sources,
    force_script_operand_sources,
    force_script_bits_sources,
    force_script_compare_sources,
    force_script_integer_sources,
    force_script_logic_sources,
    force_script_math_sources,
    force_script_range_sources,
    force_script_sample_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_output_test = executable(
    'base-force-script-output-test',
    'tests/source/force_script_output.c',
    force_script_output_sources,
    force_filter_sources,
    force_soft_stop_sources,
    force_output_scale_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_range_test = executable(
    'base-force-script-range-test',
    'tests/source/force_script_range.c',
    force_script_range_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_sample_test = executable(
    'base-force-script-sample-test',
    'tests/source/force_script_sample.c',
    force_script_sample_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_logic_test = executable(
    'base-force-script-logic-test',
    'tests/source/force_script_logic.c',
    force_script_logic_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  force_script_store_test = executable(
    'base-force-script-store-test',
    'tests/source/force_script_store.c',
    force_script_store_sources,
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

  fanatec_encoder_test = executable(
    'base-fanatec-encoder-test',
    'tests/source/fanatec_encoder.c',
    fanatec_encoder_sources,
    platform_time_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_input_report_test = executable(
    'base-usb-input-report-test',
    'tests/source/usb_input_report.c',
    usb_input_report_sources,
    fanatec_input_sources,
    logitech_input_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_compatibility_report_descriptor_test = executable(
    'base-usb-compatibility-report-descriptor-test',
    'tests/source/compatibility_report_descriptor.c',
    usb_compatibility_report_descriptor_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_compatibility_descriptor_test = executable(
    'base-usb-compatibility-descriptor-test',
    'tests/source/compatibility_descriptor.c',
    usb_compatibility_descriptor_sources,
    usb_descriptor_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_console_descriptor_test = executable(
    'base-usb-console-descriptor-test',
    'tests/source/console_descriptor.c',
    usb_console_descriptor_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_xbox_gip_metadata_test = executable(
    'base-usb-xbox-gip-metadata-test',
    'tests/source/xbox_gip_metadata.c',
    usb_xbox_gip_metadata_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_xbox_gip_metadata_download_test = executable(
    'base-usb-xbox-gip-metadata-download-test',
    'tests/source/xbox_gip_metadata_download.c',
    usb_feature_download_sources,
    usb_xbox_gip_metadata_sources,
    usb_xbox_gip_metadata_download_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_xbox_gip_discovery_test = executable(
    'base-usb-xbox-gip-discovery-test',
    'tests/source/xbox_gip_discovery.c',
    usb_xbox_gip_discovery_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_xbox_gip_session_test = executable(
    'base-usb-xbox-gip-session-test',
    'tests/source/xbox_gip_session.c',
    usb_xbox_gip_session_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_xbox_gip_response_test = executable(
    'base-usb-xbox-gip-response-test',
    'tests/source/xbox_gip_response.c',
    usb_console_descriptor_sources,
    usb_xbox_gip_response_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_xbox_gip_input_test = executable(
    'base-usb-xbox-gip-input-test',
    'tests/source/xbox_gip_input.c',
    usb_xbox_gip_input_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_xbox_gip_command_test = executable(
    'base-usb-xbox-gip-command-test',
    'tests/source/xbox_gip_command.c',
    usb_xbox_gip_command_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_xbox_gip_service_test = executable(
    'base-usb-xbox-gip-service-test',
    'tests/source/xbox_gip_service.c',
    usb_console_descriptor_sources,
    usb_feature_download_sources,
    usb_xbox_gip_discovery_sources,
    usb_xbox_gip_metadata_sources,
    usb_xbox_gip_metadata_download_sources,
    usb_xbox_gip_response_sources,
    usb_xbox_gip_service_sources,
    usb_xbox_gip_session_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_xbox_gip_vendor_tunnel_test = executable(
    'base-usb-xbox-gip-vendor-tunnel-test',
    'tests/source/xbox_gip_vendor_tunnel.c',
    usb_xbox_gip_vendor_tunnel_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_updater_descriptor_test = executable(
    'base-usb-updater-descriptor-test',
    'tests/source/updater_descriptor.c',
    usb_updater_descriptor_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_updater_control_test = executable(
    'base-usb-updater-control-test',
    'tests/source/updater_control.c',
    usb_updater_control_sources,
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
    usb_compatibility_descriptor_sources,
    usb_compatibility_report_descriptor_sources,
    usb_console_descriptor_sources,
    usb_feature_download_sources,
    usb_xbox_gip_discovery_sources,
    usb_xbox_gip_metadata_sources,
    usb_xbox_gip_metadata_download_sources,
    usb_xbox_gip_response_sources,
    usb_xbox_gip_service_sources,
    usb_xbox_gip_session_sources,
    usb_updater_descriptor_sources,
    usb_updater_control_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_motor_response_download_test = executable(
    'base-usb-motor-response-download-test',
    'tests/source/usb_motor_response_download.c',
    usb_feature_download_sources,
    usb_motor_response_download_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_feature_upload_test = executable(
    'base-usb-feature-upload-test',
    'tests/source/usb_feature_upload.c',
    usb_feature_upload_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_feature_upload_acknowledgement_test = executable(
    'base-usb-feature-upload-acknowledgement-test',
    'tests/source/usb_feature_upload_acknowledgement.c',
    usb_feature_upload_acknowledgement_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_motor_command_upload_test = executable(
    'base-usb-motor-command-upload-test',
    'tests/source/usb_motor_command_upload.c',
    usb_feature_upload_sources,
    usb_motor_command_upload_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_motor_vendor_service_test = executable(
    'base-usb-motor-vendor-service-test',
    'tests/source/usb_motor_vendor_service.c',
    usb_motor_vendor_service_sources,
    usb_motor_command_upload_sources,
    usb_motor_response_download_sources,
    usb_feature_upload_sources,
    usb_feature_download_sources,
    usb_feature_upload_acknowledgement_sources,
    motor_command_application_sources,
    motor_command_channel_sources,
    motor_command_channel_mailbox_sources,
    motor_command_mailbox_sources,
    motor_command_digest_sources,
    motor_command_fragment_sources,
    motor_command_information_sources,
    motor_command_message_sources,
    motor_command_packet_sources,
    motor_command_receiver_sources,
    motor_command_sequence_sources,
    transfer_command_sources,
    transfer_memory_sources,
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

  usb_remote_tuning_records_test = executable(
    'base-usb-remote-tuning-records-test',
    'tests/source/usb_remote_tuning_records.c',
    usb_remote_tuning_records_sources,
    remote_tuning_telemetry_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_remote_tuning_service_test = executable(
    'base-usb-remote-tuning-service-test',
    'tests/source/usb_remote_tuning_service.c',
    usb_remote_tuning_service_sources,
    usb_remote_tuning_records_sources,
    remote_tuning_telemetry_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  remote_tuning_telemetry_test = executable(
    'base-remote-tuning-telemetry-test',
    'tests/source/remote_tuning_telemetry.c',
    remote_tuning_telemetry_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_diagnostic_report_test = executable(
    'base-usb-diagnostic-report-test',
    'tests/source/usb_diagnostic_report.c',
    usb_diagnostic_report_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_tuning_profile_report_test = executable(
    'base-usb-tuning-profile-report-test',
    'tests/source/usb_tuning_profile_report.c',
    usb_tuning_profile_report_sources,
    profile_bank_sources,
    profile_tuning_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_tuning_menu_service_test = executable(
    'base-usb-tuning-menu-service-test',
    'tests/source/usb_tuning_menu_service.c',
    usb_tuning_menu_service_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_tuning_profile_service_test = executable(
    'base-usb-tuning-profile-service-test',
    'tests/source/usb_tuning_profile_service.c',
    usb_tuning_profile_service_sources,
    usb_tuning_profile_report_sources,
    profile_bank_sources,
    profile_tuning_sources,
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

  usb_connection_test = executable(
    'base-usb-connection-test',
    'tests/source/usb_connection.c',
    usb_connection_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  usb_host_capability_recovery_test = executable(
    'base-usb-host-capability-recovery-test',
    'tests/source/usb_host_capability_recovery.c',
    usb_host_capability_recovery_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  i2c_probe_test = executable(
    'base-i2c-probe-test',
    'tests/source/i2c_probe.c',
    i2c_probe_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  i2c_probe_bus_test = executable(
    'base-i2c-probe-bus-test',
    'tests/source/i2c_probe_bus.c',
    i2c_probe_sources,
    i2c_probe_bus_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  i2c_probe_startup_service_test = executable(
    'base-i2c-probe-startup-service-test',
    'tests/source/i2c_probe_startup_service.c',
    i2c_probe_sources,
    i2c_probe_startup_service_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  i2c_probe_exchange_service_test = executable(
    'base-i2c-probe-exchange-service-test',
    'tests/source/i2c_probe_exchange_service.c',
    i2c_probe_sources,
    i2c_probe_bus_sources,
    i2c_probe_exchange_service_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  i2c_probe_packet_test = executable(
    'base-i2c-probe-packet-test',
    'tests/source/i2c_probe_packet.c',
    i2c_probe_packet_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  i2c_probe_transfer_service_test = executable(
    'base-i2c-probe-transfer-service-test',
    'tests/source/i2c_probe_transfer_service.c',
    i2c_probe_sources,
    i2c_probe_bus_sources,
    i2c_probe_exchange_service_sources,
    i2c_probe_transfer_service_sources,
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

  power_controller_test = executable(
    'base-power-controller-test',
    'tests/source/power_controller.c',
    power_controller_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  torque_key_test = executable(
    'base-torque-key-test',
    'tests/source/torque_key.c',
    torque_key_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  torque_key_prompt_test = executable(
    'base-torque-key-prompt-test',
    'tests/source/torque_key_prompt.c',
    torque_key_prompt_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  system_torque_transition_test = executable(
    'base-system-torque-transition-test',
    'tests/source/system_torque_transition.c',
    system_torque_transition_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  system_event_queue_test = executable(
    'base-system-event-queue-test',
    'tests/source/system_event_queue.c',
    system_event_queue_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  system_event_dispatcher_test = executable(
    'base-system-event-dispatcher-test',
    'tests/source/system_event_dispatcher.c',
    system_event_dispatcher_sources,
    system_event_queue_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  system_notice_test = executable(
    'base-system-notice-test',
    'tests/source/system_notice.c',
    system_notice_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  system_control_state_test = executable(
    'base-system-control-state-test',
    'tests/source/system_control_state.c',
    system_control_state_sources,
    system_torque_transition_sources,
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

  h_pattern_calibration_test = executable(
    'base-h-pattern-calibration-test',
    'tests/source/h_pattern_calibration.c',
    h_pattern_calibration_sources,
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

  motor_output_transport_test = executable(
    'base-motor-output-transport-test',
    'tests/source/motor_output_transport.c',
    motor_output_transport_sources,
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
    platform_time_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_calibration_test = executable(
    'base-motor-calibration-test',
    'tests/source/motor_calibration.c',
    motor_calibration_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_status_service_test = executable(
    'base-motor-status-service-test',
    'tests/source/motor_status_service.c',
    motor_status_service_sources,
    motor_output_interlock_sources,
    motor_identity_sources,
    platform_time_sources,
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

  pedal_brake_indicator_test = executable(
    'base-pedal-brake-indicator-test',
    'tests/source/pedal_brake_indicator.c',
    pedal_brake_indicator_sources,
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

  pedal_calibration_command_test = executable(
    'base-pedal-calibration-command-test',
    'tests/source/pedal_calibration_command.c',
    pedal_calibration_command_sources,
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

  pedal_protocol_command_test = executable(
    'base-pedal-protocol-command-test',
    'tests/source/pedal_protocol_command.c',
    pedal_protocol_command_sources,
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
    auxiliary_axis_sources,
    analog_axis_sources,
    profile_record_sources,
    profile_bank_sources,
    profile_tuning_sources,
    wheel_position_sources,
    wheel_steering_limit_sources,
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

  wheel_center_capture_test = executable(
    'base-wheel-center-capture-test',
    'tests/source/wheel_center_capture.c',
    wheel_center_capture_sources,
    platform_time_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_steering_limit_test = executable(
    'base-wheel-steering-limit-test',
    'tests/source/wheel_steering_limit.c',
    wheel_steering_limit_sources,
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

  wheel_usb_disconnect_display_test = executable(
    'base-wheel-usb-disconnect-display-test',
    'tests/source/wheel_usb_disconnect_display.c',
    wheel_usb_disconnect_display_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_vibration_test = executable(
    'base-wheel-vibration-test',
    'tests/source/wheel_vibration.c',
    wheel_vibration_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_output_reports_test = executable(
    'base-wheel-output-reports-test',
    'tests/source/wheel_output_reports.c',
    wheel_output_reports_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_digest_test = executable(
    'base-motor-command-digest-test',
    'tests/source/motor_command_digest.c',
    motor_command_digest_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  transfer_memory_test = executable(
    'base-transfer-memory-test',
    'tests/source/transfer_memory.c',
    transfer_memory_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  transfer_command_test = executable(
    'base-transfer-command-test',
    'tests/source/transfer_command.c',
    transfer_command_sources,
    transfer_memory_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_packet_test = executable(
    'base-motor-command-packet-test',
    'tests/source/motor_command_packet.c',
    motor_command_packet_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_fragment_test = executable(
    'base-motor-command-fragment-test',
    'tests/source/motor_command_fragment.c',
    motor_command_fragment_sources,
    motor_command_packet_sources,
    motor_command_digest_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_mailbox_test = executable(
    'base-motor-command-mailbox-test',
    'tests/source/motor_command_mailbox.c',
    motor_command_mailbox_sources,
    transfer_command_sources,
    transfer_memory_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_sequence_test = executable(
    'base-motor-command-sequence-test',
    'tests/source/motor_command_sequence.c',
    motor_command_sequence_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_scheduler_test = executable(
    'base-motor-command-scheduler-test',
    'tests/source/motor_command_scheduler.c',
    motor_command_scheduler_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_receiver_test = executable(
    'base-motor-command-receiver-test',
    'tests/source/motor_command_receiver.c',
    motor_command_receiver_sources,
    motor_command_fragment_sources,
    motor_command_packet_sources,
    motor_command_sequence_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_message_test = executable(
    'base-motor-command-message-test',
    'tests/source/motor_command_message.c',
    motor_command_message_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_information_test = executable(
    'base-motor-command-information-test',
    'tests/source/motor_command_information.c',
    motor_command_information_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_application_test = executable(
    'base-motor-command-application-test',
    'tests/source/motor_command_application.c',
    motor_command_application_sources,
    motor_command_digest_sources,
    motor_command_information_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_startup_test = executable(
    'base-motor-command-startup-test',
    'tests/source/motor_command_startup.c',
    motor_command_startup_sources,
    transfer_command_sources,
    transfer_memory_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_startup_service_test = executable(
    'base-motor-command-startup-service-test',
    'tests/source/motor_command_startup_service.c',
    motor_command_startup_service_sources,
    motor_command_startup_sources,
    motor_command_channel_sources,
    motor_command_channel_mailbox_sources,
    motor_command_application_sources,
    motor_command_digest_sources,
    motor_command_fragment_sources,
    motor_command_information_sources,
    motor_command_mailbox_sources,
    motor_command_message_sources,
    motor_command_packet_sources,
    motor_command_receiver_sources,
    motor_command_sequence_sources,
    transfer_command_sources,
    transfer_memory_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  serial_packet_test = executable(
    'base-serial-packet-test',
    'tests/source/serial_packet.c',
    serial_packet_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  serial_message_test = executable(
    'base-serial-message-test',
    'tests/source/serial_message.c',
    serial_message_sources,
    serial_packet_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  serial_session_test = executable(
    'base-serial-session-test',
    'tests/source/serial_session.c',
    serial_session_sources,
    serial_message_sources,
    serial_packet_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  motor_command_serial_test = executable(
    'base-motor-command-serial-test',
    'tests/source/motor_command_serial.c',
    motor_command_serial_sources,
    serial_service_sources,
    serial_session_sources,
    serial_message_sources,
    serial_packet_sources,
    transfer_command_sources,
    transfer_memory_sources,
    platform_time_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  serial_service_test = executable(
    'base-serial-service-test',
    'tests/source/serial_service.c',
    serial_service_sources,
    serial_session_sources,
    serial_message_sources,
    serial_packet_sources,
    platform_time_sources,
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

  wheel_packet_mode_four_test = executable(
    'base-wheel-packet-mode-four-test',
    'tests/source/wheel_packet_mode_four.c',
    wheel_packet_mode_four_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_packet_crc_test = executable(
    'base-wheel-packet-crc-test',
    'tests/source/wheel_packet_crc.c',
    wheel_packet_crc_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_packet_remote_tuning_test = executable(
    'base-wheel-packet-remote-tuning-test',
    'tests/source/wheel_packet_remote_tuning.c',
    wheel_packet_remote_tuning_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_capability_test = executable(
    'base-wheel-capability-test',
    'tests/source/wheel_capability.c',
    wheel_capability_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_command_forwarder_test = executable(
    'base-wheel-command-forwarder-test',
    'tests/source/wheel_command_forwarder.c',
    wheel_command_forwarder_sources,
    transfer_command_sources,
    transfer_memory_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_compatibility_alert_test = executable(
    'base-wheel-compatibility-alert-test',
    'tests/source/wheel_compatibility_alert.c',
    wheel_compatibility_alert_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_motion_test = executable(
    'base-wheel-motion-test',
    'tests/source/wheel_motion.c',
    wheel_motion_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_rotary_input_test = executable(
    'base-wheel-rotary-input-test',
    'tests/source/wheel_rotary_input.c',
    wheel_rotary_input_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_protocol_test = executable(
    'base-wheel-protocol-test',
    'tests/source/wheel_protocol.c',
    wheel_protocol_sources,
    wheel_authentication_sources,
    wheel_axis_override_sources,
    wheel_capability_sources,
    wheel_motion_sources,
    wheel_output_reports_sources,
    wheel_packet_crc_sources,
    wheel_packet_mode_one_sources,
    wheel_packet_mode_four_sources,
    wheel_packet_remote_tuning_sources,
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
    wheel_capability_sources,
    wheel_motion_sources,
    wheel_rotary_input_sources,
    wheel_output_reports_sources,
    wheel_packet_crc_sources,
    wheel_packet_mode_one_sources,
    wheel_packet_mode_four_sources,
    wheel_packet_remote_tuning_sources,
    wheel_protocol_sources,
    serial_service_sources,
    serial_session_sources,
    serial_message_sources,
    serial_packet_sources,
    platform_time_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_transfer_service_test = executable(
    'base-wheel-transfer-service-test',
    'tests/source/wheel_transfer_service.c',
    wheel_transfer_service_sources,
    transfer_command_sources,
    transfer_memory_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  wheel_status_service_test = executable(
    'base-wheel-status-service-test',
    'tests/source/wheel_status_service.c',
    wheel_status_service_sources,
    serial_service_sources,
    serial_session_sources,
    serial_message_sources,
    serial_packet_sources,
    platform_time_sources,
    build_by_default: false,
    include_directories: base_include,
  )

  test('base-analog-axis', analog_axis_test, suite: ['base', 'analog'])
  test('base-auxiliary-axis', auxiliary_axis_test, suite: ['base', 'analog'])
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
  test('base-display-controller', display_controller_test, suite: ['base', 'display'])
  test('base-display-framebuffer', display_framebuffer_test, suite: ['base', 'display'])
  test('base-display-notice', display_notice_test, suite: ['base', 'display'])
  test('base-display-prompt', display_prompt_test, suite: ['base', 'display'])
  test('base-analog-samples', analog_samples_test, suite: ['base', 'analog'])
  test('base-force-effect', force_effect_test, suite: ['base', 'force-feedback'])
  test('base-force-effect-bank', force_effect_bank_test, suite: ['base', 'force-feedback'])
  test(
    'base-force-feedback-command',
    force_feedback_command_test,
    suite: ['base', 'force-feedback', 'usb', 'output'],
  )
  test(
    'base-force-feedback-state',
    force_feedback_state_test,
    suite: ['base', 'force-feedback', 'usb', 'output'],
  )
  test('base-force-controller', force_controller_test, suite: ['base', 'force-feedback'])
  test('base-force-output', force_output_test, suite: ['base', 'force-feedback'])
  test(
    'base-force-output-enable',
    force_output_enable_test,
    suite: ['base', 'force-feedback', 'output'],
  )
  test(
    'base-force-output-report',
    force_output_report_test,
    suite: ['base', 'force-feedback', 'transport'],
  )
  test(
    'base-force-output-scale',
    force_output_scale_test,
    suite: ['base', 'force-feedback', 'transport'],
  )
  test(
    'base-force-script-control',
    force_script_control_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-report',
    force_script_report_test,
    suite: ['base', 'force-feedback', 'script', 'usb'],
  )
  test(
    'base-force-script-executor',
    force_script_executor_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-service',
    force_script_service_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-scheduler',
    force_script_scheduler_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-tick',
    force_script_tick_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-runtime',
    force_script_runtime_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-bits',
    force_script_bits_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-compare',
    force_script_compare_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-input',
    force_script_input_test,
    suite: ['base', 'force-feedback', 'script', 'usb'],
  )
  test(
    'base-force-script-integer',
    force_script_integer_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-math',
    force_script_math_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-motion',
    force_script_motion_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-operand',
    force_script_operand_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-operation',
    force_script_operation_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-output',
    force_script_output_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-range',
    force_script_range_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-sample',
    force_script_sample_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-logic',
    force_script_logic_test,
    suite: ['base', 'force-feedback', 'script'],
  )
  test(
    'base-force-script-store',
    force_script_store_test,
    suite: ['base', 'force-feedback', 'script', 'usb'],
  )
  test('base-force-filter', force_filter_test, suite: ['base', 'force-feedback'])
  test('base-force-safety', force_safety_test, suite: ['base', 'force-feedback'])
  test('base-force-soft-stop', force_soft_stop_test, suite: ['base', 'force-feedback'])
  test('base-fanatec-input', fanatec_input_test, suite: ['base', 'usb', 'input'])
  test('base-fanatec-encoder', fanatec_encoder_test, suite: ['base', 'usb', 'input'])
  test('base-usb-input-report', usb_input_report_test, suite: ['base', 'usb', 'input'])
  test(
    'base-usb-compatibility-report-descriptor',
    usb_compatibility_report_descriptor_test,
    suite: ['base', 'usb'],
  )
  test(
    'base-usb-compatibility-descriptor',
    usb_compatibility_descriptor_test,
    suite: ['base', 'usb'],
  )
  test('base-usb-console-descriptor', usb_console_descriptor_test, suite: ['base', 'usb'])
  test('base-usb-xbox-gip-metadata', usb_xbox_gip_metadata_test, suite: ['base', 'usb'])
  test(
    'base-usb-xbox-gip-metadata-download',
    usb_xbox_gip_metadata_download_test,
    suite: ['base', 'usb'],
  )
  test('base-usb-xbox-gip-discovery', usb_xbox_gip_discovery_test, suite: ['base', 'usb'])
  test('base-usb-xbox-gip-session', usb_xbox_gip_session_test, suite: ['base', 'usb'])
  test('base-usb-xbox-gip-response', usb_xbox_gip_response_test, suite: ['base', 'usb'])
  test('base-usb-xbox-gip-input', usb_xbox_gip_input_test, suite: ['base', 'usb'])
  test('base-usb-xbox-gip-command', usb_xbox_gip_command_test, suite: ['base', 'usb'])
  test('base-usb-xbox-gip-service', usb_xbox_gip_service_test, suite: ['base', 'usb'])
  test(
    'base-usb-xbox-gip-vendor-tunnel',
    usb_xbox_gip_vendor_tunnel_test,
    suite: ['base', 'usb'],
  )
  test(
    'base-usb-motor-response-download',
    usb_motor_response_download_test,
    suite: ['base', 'usb', 'motor'],
  )
  test('base-usb-feature-upload', usb_feature_upload_test, suite: ['base', 'usb'])
  test(
    'base-usb-feature-upload-acknowledgement',
    usb_feature_upload_acknowledgement_test,
    suite: ['base', 'usb'],
  )
  test(
    'base-usb-motor-command-upload',
    usb_motor_command_upload_test,
    suite: ['base', 'usb', 'motor'],
  )
  test(
    'base-usb-motor-vendor-service',
    usb_motor_vendor_service_test,
    suite: ['base', 'usb', 'motor'],
  )
  test('base-usb-updater-descriptor', usb_updater_descriptor_test, suite: ['base', 'usb'])
  test('base-usb-updater-control', usb_updater_control_test, suite: ['base', 'usb'])
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
    'base-usb-remote-tuning-records',
    usb_remote_tuning_records_test,
    suite: ['base', 'usb', 'profile'],
  )
  test(
    'base-usb-remote-tuning-service',
    usb_remote_tuning_service_test,
    suite: ['base', 'usb', 'profile'],
  )
  test(
    'base-remote-tuning-telemetry',
    remote_tuning_telemetry_test,
    suite: ['base', 'remote-tuning', 'wheel'],
  )
  test('base-usb-diagnostic-report', usb_diagnostic_report_test, suite: ['base', 'usb', 'output'])
  test(
    'base-usb-tuning-menu-service',
    usb_tuning_menu_service_test,
    suite: ['base', 'usb', 'profile'],
  )
  test(
    'base-usb-tuning-profile-report',
    usb_tuning_profile_report_test,
    suite: ['base', 'usb', 'profile'],
  )
  test(
    'base-usb-tuning-profile-service',
    usb_tuning_profile_service_test,
    suite: ['base', 'usb', 'profile'],
  )
  test(
    'base-podium-report-descriptor',
    podium_report_descriptor_test,
    suite: ['base', 'usb'],
  )
  test('base-board-identity', board_identity_test, suite: ['base', 'board'])
  test('base-usb-connection', usb_connection_test, suite: ['base', 'usb', 'connection'])
  test(
    'base-usb-host-capability-recovery',
    usb_host_capability_recovery_test,
    suite: ['base', 'usb', 'xbox'],
  )
  test('base-i2c-probe', i2c_probe_test, suite: ['base', 'i2c'])
  test('base-i2c-probe-bus', i2c_probe_bus_test, suite: ['base', 'i2c'])
  test(
    'base-i2c-probe-exchange-service',
    i2c_probe_exchange_service_test,
    suite: ['base', 'i2c'],
  )
  test('base-i2c-probe-packet', i2c_probe_packet_test, suite: ['base', 'i2c', 'usb'])
  test(
    'base-i2c-probe-transfer-service',
    i2c_probe_transfer_service_test,
    suite: ['base', 'i2c'],
  )
  test(
    'base-i2c-probe-startup-service',
    i2c_probe_startup_service_test,
    suite: ['base', 'i2c'],
  )
  test('base-status-led', status_led_test, suite: ['base', 'board', 'led'])
  test('base-power-controller', power_controller_test, suite: ['base', 'board', 'power'])
  test('base-torque-key', torque_key_test, suite: ['base', 'board', 'safety'])
  test(
    'base-torque-key-prompt',
    torque_key_prompt_test,
    suite: ['base', 'system', 'safety'],
  )
  test(
    'base-system-torque-transition',
    system_torque_transition_test,
    suite: ['base', 'system', 'power'],
  )
  test('base-system-event-queue', system_event_queue_test, suite: ['base', 'system', 'event'])
  test(
    'base-system-event-dispatcher',
    system_event_dispatcher_test,
    suite: ['base', 'system', 'event'],
  )
  test('base-system-notice', system_notice_test, suite: ['base', 'system', 'display'])
  test(
    'base-system-control-state',
    system_control_state_test,
    suite: ['base', 'system', 'control'],
  )
  test('base-h-pattern-shifter', h_pattern_shifter_test, suite: ['base', 'shifter'])
  test(
    'base-h-pattern-calibration',
    h_pattern_calibration_test,
    suite: ['base', 'shifter', 'calibration'],
  )
  test('base-shifter-display', shifter_display_test, suite: ['base', 'shifter', 'display'])
  test('base-logitech-input', logitech_input_test, suite: ['base', 'usb', 'input'])
  test('base-motor-identity', motor_identity_test, suite: ['base', 'motor'])
  test('base-motor-live-frame', motor_live_frame_test, suite: ['base', 'motor', 'transport'])
  test(
    'base-motor-output-transport',
    motor_output_transport_test,
    suite: ['base', 'motor', 'transport'],
  )
  test('base-motor-output-interlock', motor_output_interlock_test, suite: ['base', 'motor'])
  test('base-motor-probe', motor_probe_test, suite: ['base', 'motor'])
  test('base-motor-calibration', motor_calibration_test, suite: ['base', 'motor', 'calibration'])
  test('base-motor-status-service', motor_status_service_test, suite: ['base', 'motor'])
  test('base-motor-telemetry-service', motor_telemetry_service_test, suite: ['base', 'motor'])
  test('base-motor-tuning-parameter', motor_tuning_parameter_test, suite: ['base', 'motor'])
  test('base-motor-tuning-sync', motor_tuning_sync_test, suite: ['base', 'motor'])
  test('base-motor-tuning-service', motor_tuning_service_test, suite: ['base', 'motor'])
  test('base-pedal-frame', pedal_frame_test, suite: ['base', 'pedal', 'transport'])
  test('base-transfer-frame', transfer_frame_test, suite: ['base', 'transfer'])
  test('base-transfer-session', transfer_session_test, suite: ['base', 'transfer'])
  test('base-pedal-axis', pedal_axis_test, suite: ['base', 'pedal', 'analog'])
  test('base-pedal-brake-indicator', pedal_brake_indicator_test, suite: ['base', 'pedal'])
  test('base-pedal-input', pedal_input_test, suite: ['base', 'pedal', 'input'])
  test(
    'base-pedal-calibration-command',
    pedal_calibration_command_test,
    suite: ['base', 'pedal', 'input'],
  )
  test('base-pedal-analog', pedal_analog_test, suite: ['base', 'pedal', 'analog'])
  test('base-pedal-protocol', pedal_protocol_test, suite: ['base', 'pedal', 'transport'])
  test(
    'base-pedal-protocol-command',
    pedal_protocol_command_test,
    suite: ['base', 'pedal', 'transport'],
  )
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
  test(
    'base-wheel-center-capture',
    wheel_center_capture_test,
    suite: ['base', 'wheel-position', 'input'],
  )
  test(
    'base-wheel-steering-limit',
    wheel_steering_limit_test,
    suite: ['base', 'wheel', 'profile'],
  )
  test('base-wheel-axis-override', wheel_axis_override_test, suite: ['base', 'wheel', 'input'])
  test(
    'base-wheel-display-output',
    wheel_display_output_test,
    suite: ['base', 'wheel', 'display'],
  )
  test(
    'base-wheel-usb-disconnect-display',
    wheel_usb_disconnect_display_test,
    suite: ['base', 'wheel', 'display', 'usb'],
  )
  test('base-wheel-vibration', wheel_vibration_test, suite: ['base', 'wheel', 'output'])
  test(
    'base-wheel-output-reports',
    wheel_output_reports_test,
    suite: ['base', 'wheel', 'output'],
  )
  test('base-motor-command-digest', motor_command_digest_test, suite: ['base', 'motor'])
  test(
    'base-transfer-memory',
    transfer_memory_test,
    suite: ['base', 'transfer'],
  )
  test(
    'base-transfer-command',
    transfer_command_test,
    suite: ['base', 'transfer'],
  )
  test(
    'base-motor-command-packet',
    motor_command_packet_test,
    suite: ['base', 'motor'],
  )
  test(
    'base-motor-command-fragment',
    motor_command_fragment_test,
    suite: ['base', 'motor'],
  )
  test(
    'base-motor-command-mailbox',
    motor_command_mailbox_test,
    suite: ['base', 'motor'],
  )
  test(
    'base-motor-command-sequence',
    motor_command_sequence_test,
    suite: ['base', 'motor'],
  )
  test(
    'base-motor-command-scheduler',
    motor_command_scheduler_test,
    suite: ['base', 'motor'],
  )
  test(
    'base-motor-command-receiver',
    motor_command_receiver_test,
    suite: ['base', 'motor'],
  )
  test(
    'base-motor-command-message',
    motor_command_message_test,
    suite: ['base', 'motor'],
  )
  test(
    'base-motor-command-information',
    motor_command_information_test,
    suite: ['base', 'motor'],
  )
  test(
    'base-motor-command-application',
    motor_command_application_test,
    suite: ['base', 'motor'],
  )
  test(
    'base-motor-command-startup',
    motor_command_startup_test,
    suite: ['base', 'motor'],
  )
  test(
    'base-motor-command-startup-service',
    motor_command_startup_service_test,
    suite: ['base', 'motor'],
  )
  test('base-serial-packet', serial_packet_test, suite: ['base', 'serial'])
  test('base-serial-message', serial_message_test, suite: ['base', 'serial'])
  test('base-serial-session', serial_session_test, suite: ['base', 'serial'])
  test('base-motor-command-serial', motor_command_serial_test, suite: ['base', 'motor'])
  test('base-serial-service', serial_service_test, suite: ['base', 'serial', 'transport'])
  test('base-wheel-protocol', wheel_protocol_test, suite: ['base', 'wheel', 'protocol'])
  test('base-wheel-capability', wheel_capability_test, suite: ['base', 'wheel', 'input'])
  test(
    'base-wheel-command-forwarder',
    wheel_command_forwarder_test,
    suite: ['base', 'wheel', 'transfer'],
  )
  test(
    'base-wheel-compatibility-alert',
    wheel_compatibility_alert_test,
    suite: ['base', 'wheel', 'safety'],
  )
  test('base-wheel-motion', wheel_motion_test, suite: ['base', 'wheel', 'input'])
  test('base-wheel-rotary-input', wheel_rotary_input_test, suite: ['base', 'wheel', 'input'])
  test(
    'base-wheel-packet-crc',
    wheel_packet_crc_test,
    suite: ['base', 'wheel', 'protocol'],
  )
  test(
    'base-wheel-packet-remote-tuning',
    wheel_packet_remote_tuning_test,
    suite: ['base', 'wheel', 'protocol'],
  )
  test(
    'base-wheel-packet-mode-one',
    wheel_packet_mode_one_test,
    suite: ['base', 'wheel', 'protocol'],
  )
  test(
    'base-wheel-packet-mode-four',
    wheel_packet_mode_four_test,
    suite: ['base', 'wheel', 'protocol'],
  )
  test('base-wheel-service', wheel_service_test, suite: ['base', 'wheel', 'input'])
  test('base-wheel-status-service', wheel_status_service_test, suite: ['base', 'wheel', 'status'])
  test(
    'base-wheel-transfer-service',
    wheel_transfer_service_test,
    suite: ['base', 'wheel', 'transfer'],
  )
endif
