python = find_program('python', required: false)

tests_dir = meson.current_source_dir()
tests_env = environment()

test('small_file_test', python, args: [join_paths(tests_dir, 'small_file_test.py')], env: tests_env, timeout: 0, verbose: false)
test('file_test', python, args: [join_paths(tests_dir, 'file_test.py')], env: tests_env, timeout: 0, verbose: false)
test('file_pe_test', python, args: [join_paths(tests_dir, 'file_pe_test.py')], env: tests_env, timeout: 0, verbose: false)
test('folder_test', python, args: [join_paths(tests_dir, 'folder_test.py')], env: tests_env, timeout: 0, verbose: false)
test('folder_pe_test', python, args: [join_paths(tests_dir, 'folder_pe_test.py')], env: tests_env, timeout: 0, verbose: false)