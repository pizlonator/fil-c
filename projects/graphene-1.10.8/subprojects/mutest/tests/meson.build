tests = [
  'general',
  'hooks',
  'types',
]

foreach t: tests
  bin = executable(t, t + '.c', dependencies: mutest_dep)
  test(t, bin)
endforeach
