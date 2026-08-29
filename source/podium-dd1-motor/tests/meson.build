motor_current_calibration_test = executable(
  motor_test_prefix + '-current-calibration-test',
  'source/current_calibration.c',
  motor_current_offset_calibration_sources,
  include_directories: firmware_include,
)

motor_control_test = executable(
  motor_test_prefix + '-control-test',
  'source/control.c',
  motor_control_state_sources,
  include_directories: firmware_include,
)

motor_foc_test = executable(
  motor_test_prefix + '-foc-test',
  'source/foc.c',
  '../source/motor/foc.c',
  include_directories: [include_directories('stubs'), firmware_include],
)

test(
  motor_test_prefix + '-foc',
  motor_foc_test,
  suite: ['motor', 'control'],
)

test(
  motor_test_prefix + '-control',
  motor_control_test,
  suite: ['motor', 'control'],
)

motor_center_test = executable(
  motor_test_prefix + '-center-test',
  'source/center.c',
  motor_center_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-center',
  motor_center_test,
  suite: ['motor', 'encoder'],
)

motor_encoder_test = executable(
  motor_test_prefix + '-encoder-test',
  'source/encoder.c',
  motor_encoder_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-encoder',
  motor_encoder_test,
  suite: ['motor', 'encoder'],
)

motor_encoder_calibration_test = executable(
  motor_test_prefix + '-encoder-calibration-test',
  'source/encoder_calibration.c',
  motor_encoder_calibration_sources,
  motor_motion_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-encoder-calibration',
  motor_encoder_calibration_test,
  suite: ['motor', 'encoder'],
)

test(
  motor_test_prefix + '-current-calibration',
  motor_current_calibration_test,
  suite: ['motor', 'calibration'],
)

motor_service_timing_test = executable(
  motor_test_prefix + '-service-timing-test',
  'source/service_timing.c',
  motor_service_sources,
  include_directories: firmware_include,
)

motor_velocity_control_test = executable(
  motor_test_prefix + '-velocity-control-test',
  'source/velocity_control.c',
  motor_velocity_control_sources,
  include_directories: [include_directories('stubs'), firmware_include],
)

test(
  motor_test_prefix + '-velocity-control',
  motor_velocity_control_test,
  suite: ['motor', 'control'],
)

test(
  motor_test_prefix + '-service-timing',
  motor_service_timing_test,
  suite: ['motor', 'service'],
)

motor_force_feedback_test = executable(
  motor_test_prefix + '-force-feedback-test',
  'source/force_feedback.c',
  motor_force_feedback_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-force-feedback',
  motor_force_feedback_test,
  suite: ['motor', 'force-feedback'],
)

motor_link_frame_test = executable(
  motor_test_prefix + '-link-frame-test',
  'source/link_frame.c',
  motor_link_frame_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-link-frame',
  motor_link_frame_test,
  suite: ['motor', 'link'],
)

motor_drive_test = executable(
  motor_test_prefix + '-drive-test',
  'source/drive.c',
  motor_drive_sources,
  motor_motion_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-drive',
  motor_drive_test,
  suite: ['motor', 'drive'],
)

motor_hardware_profile_test = executable(
  motor_test_prefix + '-hardware-profile-test',
  'source/hardware_profile.c',
  motor_hardware_profile_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-hardware-profile',
  motor_hardware_profile_test,
  suite: ['motor', 'hardware'],
)

motor_motion_test = executable(
  motor_test_prefix + '-motion-test',
  'source/motion.c',
  motor_motion_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-motion',
  motor_motion_test,
  suite: ['motor', 'motion'],
)

motor_telemetry_test = executable(
  motor_test_prefix + '-telemetry-test',
  'source/telemetry.c',
  motor_auxiliary_telemetry_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-telemetry',
  motor_telemetry_test,
  suite: ['motor', 'telemetry'],
)

motor_parameter_test = executable(
  motor_test_prefix + '-parameter-test',
  'source/parameter.c',
  motor_parameter_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-parameter',
  motor_parameter_test,
  suite: ['motor', 'communication'],
)

motor_protocol_test = executable(
  motor_test_prefix + '-protocol-test',
  'source/protocol.c',
  motor_protocol_sources,
  motor_drive_sources,
  motor_force_feedback_sources,
  motor_link_frame_sources,
  motor_motion_sources,
  include_directories: firmware_include,
)

test(
  motor_test_prefix + '-protocol',
  motor_protocol_test,
  suite: ['motor', 'communication'],
)
