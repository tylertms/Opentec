protocol_include = include_directories('source')

protocol_library = static_library(
  'wqr-protocol',
  'source/protocol.c',
  include_directories: protocol_include,
)

protocol_dependency = declare_dependency(
  include_directories: protocol_include,
  link_with: protocol_library,
)

if not meson.is_cross_build()
  protocol_test = executable(
    'wqr-protocol-test',
    'tests/protocol_test.c',
    dependencies: protocol_dependency,
  )

  test('wqr-protocol', protocol_test)
endif
