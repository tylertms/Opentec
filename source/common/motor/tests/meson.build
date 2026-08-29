motor_current_calibration_test = executable(
  'motor-current-calibration-test',
  'source/current_calibration.c',
  motor_current_offset_calibration_sources,
  include_directories: common_include,
)

test(
  'motor-current-calibration',
  motor_current_calibration_test,
  suite: ['motor', 'calibration'],
)

motor_service_timing_test = executable(
  'motor-service-timing-test',
  'source/service_timing.c',
  motor_service_sources,
  include_directories: common_include,
)

test(
  'motor-service-timing',
  motor_service_timing_test,
  suite: ['motor', 'service'],
)

motor_force_feedback_test = executable(
  'motor-force-feedback-test',
  'source/force_feedback.c',
  motor_force_feedback_sources,
  include_directories: common_include,
)

test(
  'motor-force-feedback',
  motor_force_feedback_test,
  suite: ['motor', 'force-feedback'],
)

motor_link_frame_test = executable(
  'motor-link-frame-test',
  'source/link_frame.c',
  motor_link_frame_sources,
  include_directories: common_include,
)

test(
  'motor-link-frame',
  motor_link_frame_test,
  suite: ['motor', 'link'],
)

motor_drive_test = executable(
  'motor-drive-test',
  'source/drive.c',
  motor_drive_sources,
  include_directories: common_include,
)

test(
  'motor-drive',
  motor_drive_test,
  suite: ['motor', 'drive'],
)

motor_hardware_profile_test = executable(
  'motor-hardware-profile-test',
  'source/hardware_profile.c',
  motor_hardware_profile_sources,
  include_directories: common_include,
)

test(
  'motor-hardware-profile',
  motor_hardware_profile_test,
  suite: ['motor', 'hardware'],
)

motor_motion_test = executable(
  'motor-motion-test',
  'source/motion.c',
  motor_motion_sources,
  include_directories: common_include,
)

test(
  'motor-motion',
  motor_motion_test,
  suite: ['motor', 'motion'],
)

motor_telemetry_test = executable(
  'motor-telemetry-test',
  'source/telemetry.c',
  motor_auxiliary_telemetry_sources,
  include_directories: common_include,
)

test(
  'motor-telemetry',
  motor_telemetry_test,
  suite: ['motor', 'telemetry'],
)

motor_parameter_test = executable(
  'motor-parameter-test',
  'source/parameter.c',
  motor_parameter_sources,
  include_directories: common_include,
)

test(
  'motor-parameter',
  motor_parameter_test,
  suite: ['motor', 'communication'],
)

motor_protocol_test = executable(
  'motor-protocol-test',
  'source/protocol.c',
  motor_protocol_sources,
  motor_drive_sources,
  motor_force_feedback_sources,
  motor_link_frame_sources,
  include_directories: common_include,
)

test(
  'motor-protocol',
  motor_protocol_test,
  suite: ['motor', 'communication'],
)
