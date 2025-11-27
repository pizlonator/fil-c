annotationparser_test_env = environment()
annotationparser_test_env.prepend('PYTHONPATH', test_env_common_pypath)
if test_env_common_path.length() > 0
    annotationparser_test_env.prepend('PATH', test_env_common_path)
endif

annotationparser_test_files = [
  'test_patterns.py',
  'test_parser.py',
]

foreach f : annotationparser_test_files
  test(f, python, args: files(f), env: annotationparser_test_env, depends: [giscanner_pymod])
endforeach
