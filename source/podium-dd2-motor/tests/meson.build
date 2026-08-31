motor_tests = [
  {
    'name': 'current-calibration',
    'suite': 'calibration',
    'sources': ['source/current_calibration.c', '../source/motor/calibration.c'],
  },
  {
    'name': 'control',
    'suite': 'control',
    'sources': ['source/control.c', '../source/motor/control.c'],
  },
  {
    'name': 'velocity-control',
    'suite': 'control',
    'sources': ['source/velocity_control.c', '../source/motor/velocity_control.c'],
  },
  {
    'name': 'center',
    'suite': 'encoder',
    'sources': ['source/center.c', '../source/motor/center.c'],
  },
  {
    'name': 'encoder',
    'suite': 'encoder',
    'sources': ['source/encoder.c', '../source/motor/encoder.c'],
  },
  {
    'name': 'encoder-calibration',
    'suite': 'encoder',
    'sources': [
      'source/encoder_calibration.c',
      '../source/motor/encoder_calibration.c',
      '../source/motor/motion.c',
    ],
  },
  {
    'name': 'service-timing',
    'suite': 'service',
    'sources': ['source/service_timing.c', '../source/system/timing.c'],
  },
  {
    'name': 'force-feedback',
    'suite': 'force-feedback',
    'sources': [
      'source/force_feedback.c',
      '../source/force_feedback/effect.c',
      '../source/force_feedback/command.c',
      '../source/force_feedback/engine.c',
      '../source/force_feedback/soft_stop.c',
    ],
  },
  {
    'name': 'link-frame',
    'suite': 'link',
    'sources': ['source/link_frame.c', '../source/link/frame.c'],
  },
  {
    'name': 'drive',
    'suite': 'drive',
    'sources': ['source/drive.c', '../source/motor/drive.c', '../source/motor/motion.c'],
  },
  {
    'name': 'hardware-profile',
    'suite': 'hardware',
    'sources': ['source/hardware_profile.c', '../source/profile/hardware.c'],
  },
  {
    'name': 'motion',
    'suite': 'motion',
    'sources': ['source/motion.c', '../source/motor/motion.c'],
  },
  {
    'name': 'telemetry',
    'suite': 'telemetry',
    'sources': ['source/telemetry.c', '../source/telemetry/auxiliary.c'],
  },
  {
    'name': 'parameter',
    'suite': 'communication',
    'sources': ['source/parameter.c', '../source/tuning/parameter.c'],
  },
  {
    'name': 'protocol',
    'suite': 'communication',
    'sources': [
      'source/protocol.c',
      '../source/link/protocol.c',
      '../source/motor/drive.c',
      '../source/force_feedback/effect.c',
      '../source/force_feedback/command.c',
      '../source/force_feedback/engine.c',
      '../source/force_feedback/soft_stop.c',
      '../source/link/frame.c',
      '../source/motor/motion.c',
    ],
  },
]

foreach definition : motor_tests
  sources = []
  foreach source : definition['sources']
    sources += files(source)
  endforeach
  test_includes = motor_include
  if definition['name'] == 'velocity-control'
    test_includes = [include_directories('stubs'), motor_include]
  endif
  image = executable(
    motor_test_prefix + '-' + definition['name'] + '-test',
    sources,
    build_by_default: false,
    include_directories: test_includes,
  )
  test(
    motor_test_prefix + '-' + definition['name'],
    image,
    suite: ['motor', definition['suite']],
  )
endforeach
