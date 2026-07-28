# SPDX-License-Identifier: Apache-2.0
#
# grok-policyd: library TCB only (no nng Cap'n serve process).
# Host C unit tests: cmocka (one suite binary).

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
  'third_party/cwalk',
  'third_party/bsdcompat/include',
)

cmocka_dep = dependency('cmocka', required: true)

check_version = find_program('scripts/check-version.sh', required: true)
run_command(check_version, check: true)

vendor_srcs = files(
  'third_party/cwalk/cwalk.c',
  'third_party/bsdcompat/strlcpy.c',
  'third_party/bsdcompat/strlcat.c',
)

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
  files(
    'tests/harness.c',
    'tests/test_paths.c',
    'tests/test_lifecycle.c',
    'tests/test_kill_tree.c',
    'tests/test_action_log.c',
    'tests/test_persist.c',
    'tests/test_policy.c',
    'tests/test_version.c',
    'tests/test_main.c',
  ),
  include_directories: inc,
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
