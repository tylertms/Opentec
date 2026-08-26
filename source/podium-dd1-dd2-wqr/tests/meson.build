protocol_test = executable(
  'wqr-protocol-test',
  'protocol_test.c',
  sources,
  build_by_default: false,
  include_directories: include,
)

test('wqr-protocol', protocol_test, suite: ['wqr', 'protocol'])
