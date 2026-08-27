protocol_test = executable(
  'wqr-protocol-test',
  'protocol_test.c',
  protocol_sources,
  build_by_default: false,
  include_directories: protocol_include,
)

test('wqr-protocol', protocol_test, suite: ['wqr', 'protocol'])
