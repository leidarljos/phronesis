# SPDX-License-Identifier: Apache-2.0
#
# grok-policyd: Meson is the ONLY build system.
# Host C unit tests: cmocka ONLY (one suite binary).
#
# Portability layers:
#   libgrok_policyd  — TCB (process/policy; Linux process model today)
#   Cap'n + nng wire — portable protocol; serve is nng-native
#   host_*           — optional seat glue (systemd notify); not required for serve

project(
  'grok-policyd',
  'c',
  version: run_command(
    'bash',
    '-c',
    'tr -d "[:space:]" < VERSION',
    check: true,
  ).stdout().strip(),
  license: 'Apache-2.0',
  meson_version: '>=1.1.0',
  default_options: [
    'c_std=gnu11',
    'warning_level=2',
    'werror=true',
    'buildtype=release',
    'default_library=both',
  ],
)

fs = import('fs')
pkg = import('pkgconfig')

ver = meson.project_version()
ver_bits = ver.split('.')
soversion = ver_bits[0]

cc = meson.get_compiler('c')
add_project_arguments(
  '-D_GNU_SOURCE',
  language: 'c',
)

inc = include_directories(
  'include',
  'src',
  'tests',
  'third_party/cwalk',
  'third_party/bsdcompat/include',
)

# --- always-required deps for wire/CLI ---
# conda-forge nng often ships lib+headers without nng.pc (CI #14656).
# pkg-config only first (skip flaky CMake/MbedTLS configs). Fallback:
# find_library('nng') under the active conda/pixi compiler search path.
nng_dep = dependency('nng', method: 'pkg-config', required: false)
if not nng_dep.found()
  nng_lib = cc.find_library('nng', required: true)
  if not cc.has_header('nng/nng.h')
    error('nng library found but nng/nng.h is missing (CONDA_PREFIX/include on CPATH?)')
  endif
  nng_dep = declare_dependency(dependencies: nng_lib)
  message('nng: find_library fallback (no nng.pc)')
else
  message('nng: pkg-config')
endif
capnp_c_dep = dependency('c-capnproto', required: true)
cmocka_dep = dependency('cmocka', required: true)

# --- optional seat host (Linux defaults auto) ---
systemd_dep = dependency('libsystemd', required: get_option('systemd'))
cap_dep = dependency('libcap', required: get_option('libcap'))
have_systemd = systemd_dep.found()
have_libcap = cap_dep.found()

if have_libcap and not have_systemd
  warning('libcap enabled without systemd: caps drop still works; notify is posix no-op host')
endif

capnp = find_program('capnp', required: true)
capnpc_c = find_program('capnpc-c', required: true)
gen_capnp = find_program('scripts/gen-capnp-c.sh', required: true)

check_version = find_program('scripts/check-version.sh', required: true)
run_command(check_version, check: true)

schema_gen = custom_target(
  'policy_capnp',
  input: files('schema/policy.capnp'),
  output: ['policy.capnp.c', 'policy.capnp.h'],
  command: [gen_capnp, '@INPUT@', '@OUTPUT0@', '@OUTPUT1@', capnpc_c],
  build_by_default: true,
)

schema_inc = include_directories('.')

vendor_srcs = files(
  'third_party/cwalk/cwalk.c',
  'third_party/bsdcompat/strlcpy.c',
  'third_party/bsdcompat/strlcat.c',
)

# --- TCB library: NO nng / systemd / cap / c-capnproto ---
lib_srcs = files(
  'src/paths.c',
  'src/unix_dir.c',
  'src/action_log.c',
  'src/cgroup.c',
  'src/policy.c',
  'src/supervisor.c',
  'src/version.c',
) + vendor_srcs

policyd_lib = library(
  'grok_policyd',
  lib_srcs,
  include_directories: inc,
  version: ver,
  soversion: soversion,
  install: true,
  gnu_symbol_visibility: 'default',
)

install_headers(
  'include/grok-policyd/supervisor.h',
  subdir: 'grok-policyd',
)

pkg.generate(
  policyd_lib,
  name: 'grok-policyd',
  description: 'GrokOS multi-agent policy supervisor (stable C ABI)',
  url: 'https://nova.teachx.ai/trace-analysis/grokos-packages/grok-policyd',
  filebase: 'grok-policyd',
  subdirs: '.',
)

# --- host backend: one of posix | linux-systemd ---
if have_systemd
  host_srcs = files('src/host_linux_systemd.c')
else
  host_srcs = files('src/host_posix.c')
endif

wire_srcs = files(
  'src/wire/codec_c_capn.c',
  'src/wire/serve.c',
) + host_srcs

host_dep = [nng_dep, capnp_c_dep]
if have_systemd
  host_dep += [systemd_dep]
endif
if have_libcap
  host_dep += [cap_dep]
endif

host_c_args = ['-Wno-unused-parameter']
if have_libcap
  host_c_args += ['-DGROK_HAVE_LIBCAP=1']
endif

wire_lib = static_library(
  'policyd_wire',
  wire_srcs + [schema_gen],
  include_directories: [inc, schema_inc],
  dependencies: host_dep,
  c_args: host_c_args,
  install: false,
)

cli = executable(
  'grok-policyd',
  files('src/grok-policyd.c'),
  include_directories: [inc, schema_inc],
  link_with: [policyd_lib, wire_lib],
  dependencies: host_dep,
  install: true,
)

example = executable(
  'example_minimal',
  files('examples/c/minimal.c'),
  include_directories: inc,
  link_with: policyd_lib,
  install: false,
)

test_srcs = files(
  'tests/harness.c',
  'tests/test_paths.c',
  'tests/test_lifecycle.c',
  'tests/test_kill_tree.c',
  'tests/test_action_log.c',
  'tests/test_persist.c',
  'tests/test_policy.c',
  'tests/test_version.c',
  'tests/test_wire_serve.c',
  'tests/test_wire_frame.c',
  'tests/test_main.c',
)

supervisor_test = executable(
  'supervisor_test',
  test_srcs,
  include_directories: [inc, schema_inc],
  link_with: [policyd_lib, wire_lib],
  dependencies: host_dep + [cmocka_dep],
  c_args: ['-Wno-format-truncation'] + host_c_args,
  install: false,
)

test(
  'cmocka',
  supervisor_test,
  protocol: 'exitcode',
  timeout: 120,
  workdir: meson.project_source_root(),
  env: {
    'POLICYD_BIN': cli.full_path(),
  },
  depends: [cli],
)

bash = find_program('bash', required: true)
test(
  'check_shared_libs',
  bash,
  args: [meson.project_source_root() / 'scripts/check_shared_libs.sh'],
  env: {
    'POLICYD_BUILD_DIR': meson.project_build_root(),
  },
  depends: [policyd_lib],
  workdir: meson.project_source_root(),
)
test(
  'smoke_serve_signal',
  bash,
  args: [meson.project_source_root() / 'scripts/smoke_serve_signal.sh'],
  env: {
    'POLICYD_BIN': cli.full_path(),
  },
  depends: [cli],
  timeout: 30,
  workdir: meson.project_source_root(),
)

# Summary for configure log
summary({
  'host backend': have_systemd ? 'linux-systemd' : 'posix',
  'libcap': have_libcap,
  'systemd': have_systemd,
  'nng serve': 'native (RECV timeout + stop flag)',
}, section: 'Portability')

alias_target('lib', policyd_lib)
alias_target('cli', cli)
alias_target('example', example)
alias_target('tests', supervisor_test)
