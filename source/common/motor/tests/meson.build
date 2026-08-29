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
