project('loongdiff', 'cpp',
    version : '0.1',
    default_options : [
        'warning_level=3',
        'cpp_std=c++20',
        'b_vscrt=static_from_buildtype',
        # 'backend_startup_project=ldiff'
    ])

add_global_arguments(['/Zi', '/FS'], language: 'cpp')
add_global_link_arguments(['/DEBUG:FULL'], language: 'cpp')

windows = import('windows')
cmake = import('cmake')

CLI11_dep = dependency('CLI11', required: true)
nlohmann_json_dep = dependency('nlohmann_json')

conf_data = configuration_data()
conf_data.set('version', meson.project_version())
configure_file(input: 'config.h.in', output: 'config.h', configuration: conf_data)
configuration_inc = include_directories('.')

utils_src_files = files([
    join_paths('src', 'file.ixx'),
    join_paths('src', 'fmt.ixx'),
])

src_files = files([
    join_paths('src', 'traits.hpp'),
    join_paths('src', 'pbar.hpp'),
    join_paths('src', 'tool.hpp'),
    join_paths('src', 'core.hpp'),
    join_paths('src', 'ldiff.cc'),
])

utils_static_lib = static_library('utils', sources: [utils_src_files])

ldiff_exe = executable('ldiff',
    sources: [src_files, windows.compile_resources('assets/ldiff.rc')],
    include_directories: ['src', configuration_inc],
    dependencies: [
        CLI11_dep,
        nlohmann_json_dep
    ],
    link_with: [utils_static_lib],
    install : true,
    install_dir: join_paths(meson.current_source_dir(), 'dist'))

subdir('tests')
