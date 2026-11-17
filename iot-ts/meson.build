# Copyright (C) 2026 Interpretica Unipessoal Lda
project('iot-ts', 'c',
    version: '1.0.0',
    default_options: 'werror=true'
)
te_path = get_option('te_path')
te_libdir = get_option('te_libdir')
add_project_arguments(get_option('te_cflags').split(), language: 'c')
add_project_link_arguments(get_option('te_ldflags').split(), language: 'c')

cc = meson.get_compiler('c')

te_tests_info_sh = find_program(join_paths(te_path, 'te_tests_info.sh'))

test_deps = [
    dependency('threads'),
    dependency('openssl'),
    dependency('zlib'),
    cc.find_library('m'),
]

# Libraries under test come first: a dependant must be named before what
# it depends on. Every tapi_<technology> below reports through
# tapi_cybersec (or, for cwmp/time, builds on tapi_devtool), which builds
# on tapi_kernel, on tapi_devtool.
te_libs = [
    # IoT protocol / codec / discovery / bus technologies
    'tapi_coap',
    'tapi_mdns',
    'tapi_ics',
    'tapi_gpio',
    'tapi_sensors',
    'tapi_watchdog',
    'tapi_mqtt',
    'tapi_lorawan',
    'tapi_802154',
    'tapi_bt',
    'tapi_cwmp',
    'tapi_cfg_wifi',
    # Cross-cutting security layer
    'tapi_fuzz',
    'tapi_tls',
    'tapi_time',
    # Foundation chain
    'tapi_cybersec',
    'tapi_kernel',
    'tapi_devtool',
    # TE core
    'tapi_env',
    'tapi_job',
    'tapi_rpc',
    'rcfrpc',
    'tapi',
    'confapi',
    'conf_oid',
    'rcfapi',
    'rpcxdr',
    'rpc_types',
    'logger_ten',
    'ipc',
    'tools',
    'tapi_tad',
    'ndn',
    'asn',
    'logger_core',
]

foreach lib : te_libs
    test_deps += cc.find_library(lib, dirs: te_libdir)
endforeach

lib_dir = include_directories('lib')

# The line below would produce empty dependencies on systems
# having no tirpc - it is not a problem.
dep_tirpc = dependency('libtirpc', required: false)

test_deps += [ dep_tirpc ]

subdir('lib')

test_deps += declare_dependency(include_directories: lib_dir,
                                link_with: [ ts_lib ])


tests = [ 'prologue', 'epilogue' ]

package_dir = 'iot-ts'
package_tests_c = []

foreach test : tests
    test_exe = test
    test_c = test + '.c'
    package_tests_c += [ test_c ]
    executable(test_exe, test_c, install: true, install_dir: package_dir,
               dependencies: test_deps)
endforeach

install_data([ 'mainpage.dox', 'package.xml' ],
             install_dir: package_dir)

mydir = package_dir

# One block per group: subdir() takes a literal, and each group resets
# package_dir and package_tests_c for the block below it.
package_dir = join_paths(mydir, 'mqtt')
package_tests_c = []
subdir('mqtt')

package_dir = join_paths(mydir, 'coap')
package_tests_c = []
subdir('coap')

package_dir = join_paths(mydir, 'lorawan')
package_tests_c = []
subdir('lorawan')

package_dir = join_paths(mydir, 'ieee802154')
package_tests_c = []
subdir('ieee802154')

package_dir = join_paths(mydir, 'mdns')
package_tests_c = []
subdir('mdns')

package_dir = join_paths(mydir, 'ble')
package_tests_c = []
subdir('ble')

package_dir = join_paths(mydir, 'wifi')
package_tests_c = []
subdir('wifi')

package_dir = join_paths(mydir, 'gpio')
package_tests_c = []
subdir('gpio')

package_dir = join_paths(mydir, 'sensors')
package_tests_c = []
subdir('sensors')

package_dir = join_paths(mydir, 'watchdog')
package_tests_c = []
subdir('watchdog')

package_dir = join_paths(mydir, 'ics')
package_tests_c = []
subdir('ics')

package_dir = join_paths(mydir, 'cwmp')
package_tests_c = []
subdir('cwmp')

package_dir = join_paths(mydir, 'security')
package_tests_c = []
subdir('security')
