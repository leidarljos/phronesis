# SPDX-License-Identifier: Apache-2.0
#
# grok-policyd: Cap'n language in-process (lib) + cmocka.
# No nng policyd.sock server. Callers FFI with policy.capnp bodies.

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

pkg = import('pkgconfig')
ver = meson.project_version()
soversion = ver.split('.')[0]

cc = meson.get_compiler('c')
add_project_arguments('-D_GNU_SOURCE', language: 'c')

inc = include_directories(
  'include',
  'src',
  'tests',
  'third_party/c-capnproto/lib',
)

cmocka_dep = dependency('cmocka', required: true)
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

# Pure-C Cap'n runtime (vendored) — linked into the TCB so FFI speaks Cap'n.
capn_runtime = files(
  'third_party/c-capnproto/lib/capn.c',
  'third_party/c-capnproto/lib/capn-malloc.c',
  'third_party/c-capnproto/lib/capn-stream.c',
)

lib_srcs = files(
  'src/paths.c',
  'src/action_log.c',
  'src/cgroup.c',
  'src/policy.c',
  'src/supervisor.c',
  'src/version.c',
  'src/capnp_api.c',
) + capn_runtime + [schema_gen]

policyd_lib = library(
  'grok_policyd',
  lib_srcs,
  include_directories: [inc, schema_inc],
  version: ver,
  soversion: soversion,
  install: true,
  gnu_symbol_visibility: 'default',
)

install_headers(
  'include/grok-policyd/supervisor.h',
  subdir: 'grok-policyd',
)

# Install schema SoT for embedders / code generation.
install_data('schema/policy.capnp', install_dir: get_option('datadir') / 'grok-policyd')

pkg.generate(
  policyd_lib,
  name: 'grok-policyd',
  description: 'GrokOS multi-agent policy TCB (Cap\'n FFI + C ABI)',
  url: 'https://nova.teachx.ai/trace-analysis/grokos-packages/grok-policyd',
  filebase: 'grok-policyd',
  subdirs: '.',
)

cli = executable(
  'grok-policyd',
  files('src/grok-policyd.c'),
  include_directories: inc,
  link_with: policyd_lib,
  install: true,
)

example = executable(
  'example_minimal',
  files('examples/c/minimal.c'),
  include_directories: inc,
  link_with: policyd_lib,
  install: false,
)

supervisor_test = executable(
  'supervisor_test',
  [schema_gen] + files(
    'tests/harness.c',
    'tests/test_paths.c',
    'tests/test_lifecycle.c',
    'tests/test_kill_tree.c',
    'tests/test_action_log.c',
    'tests/test_persist.c',
    'tests/test_policy.c',
    'tests/test_version.c',
    'tests/test_wire_frame.c',
    'tests/test_wire_serve.c',
    'tests/test_main.c',
  ),
  include_directories: [inc, schema_inc],
  link_with: policyd_lib,
  dependencies: [cmocka_dep],
  c_args: ['-Wno-format-truncation'],
  install: false,
)

test(
  'cmocka',
  supervisor_test,
  protocol: 'exitcode',
  timeout: 120,
  workdir: meson.project_source_root(),
)

bash = find_program('bash', required: true)
test(
  'check_shared_libs',
  bash,
  args: [meson.project_source_root() / 'scripts/check_shared_libs.sh'],
  env: {'POLICYD_BUILD_DIR': meson.project_build_root()},
  depends: [policyd_lib],
  workdir: meson.project_source_root(),
)

alias_target('lib', policyd_lib)
alias_target('cli', cli)
alias_target('example', example)
alias_target('tests', supervisor_test)
