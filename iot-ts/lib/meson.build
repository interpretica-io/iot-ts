# Copyright (C) 2026 Interpretica Unipessoal Lda

# Each agent-side technology needs the same thing: an RPC server on the
# agent, because its tapi_<x> drives a native library (libcoap, avahi,
# libmodbus, libgpiod, libsensors, the watchdog uapi, libupnp) over that
# RPC server. One tiny session helper per technology makes it. tsapi_evo
# is kept only for suites that want the downstream EVO feed; charts use
# te_mi_log (libtools) directly.
sources = [
    'tsapi_cybersec.c',
    'tsapi_evo.c',
    'tsapi_coap.c',
    'tsapi_mdns.c',
    'tsapi_ics.c',
    'tsapi_gpio.c',
    'tsapi_sensors.c',
    'tsapi_watchdog.c',
]

ts_lib = static_library('ts_lib', sources,
                        dependencies: test_deps,
                        include_directories: lib_dir)
