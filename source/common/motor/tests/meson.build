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
