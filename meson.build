project(
  'opentec',
  'c',
  meson_version: '>=1.12.0',
  default_options: [
    'build.c_std=c11',
    'c_std=c11',
    'warning_level=3',
    'werror=true',
  ],
)

if (
  meson.is_cross_build()
  and host_machine.cpu_family() == 'arm'
  and host_machine.cpu() == 'cortex-m4'
)
  add_languages('c', native: true)
endif

common_include = include_directories('source')
motor_current_offset_calibration_sources = files('source/common/motor/calibration.c')
motor_force_feedback_sources = files(
  'source/common/motor/force_feedback.c',
  'source/common/motor/force_feedback_command.c',
  'source/common/motor/force_feedback_engine.c',
  'source/common/motor/force_feedback_soft_stop.c',
)
motor_hardware_profile_sources = files('source/common/motor/profile.c')
motor_link_frame_sources = files('source/common/motor/link_frame.c')
motor_service_sources = files('source/common/motor/service.c')
motor_control_sources = files(
  'source/common/motor/board.c',
  'source/common/motor/communication.c',
  'source/common/motor/foc.c',
  'source/common/motor/io.c',
  'source/common/motor/link.c',
  'source/common/motor/peripherals.c',
  'source/common/motor/spi.c',
) + motor_current_offset_calibration_sources + motor_force_feedback_sources + motor_hardware_profile_sources + motor_link_frame_sources + motor_service_sources
wqr_frame_sources = files('source/common/wqr_frame.c')

subdir('source/podium-dd1-dd2-wqr')
subdir('source/podium-dd1-dd2-base')
subdir('source/podium-dd1-motor')
subdir('source/podium-dd2-motor')

if not meson.is_cross_build()
  subdir('source/common/motor/tests')
endif
