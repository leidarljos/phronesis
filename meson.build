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
)

# cmocka for unit tests (CI + Mull). Library itself only needs c-capnproto.
cmocka_dep = dependency('cmocka', required: true)
# Cap'n pure-C runtime from the c-capnproto package (libcapnp_c). Do not
# vendor/static-compile capn-malloc into the TCB — that path bit-rotted.
capn_c_dep = dependency('c-capnproto', required: true)
capnp = find_program('capnp', required: true)
capnpc_c = find_program('capnpc-c', required: true)
gen_capnp = find_program('scripts/gen-capnp-c.sh', required: true)

check_version = find_program('scripts/check-version.sh', required: true)
run_command(check_version, check: true)

fs = import('fs')

# --- Mull mutation testing (optional, Clang-only) -------------------------
# Enable:  CC=clang-19 meson setup build-mull -Dmutation=true -Dbuildtype=debug
# Run:     meson compile -C build-mull mutation
mull_c_args = []
if get_option('mutation')
  if cc.get_id() != 'clang'
    error('mutation=true requires Clang as the C compiler (set CC=clang-N)')
  endif
  mull_runner = find_program(
    'mull-runner-19',
    'mull-runner-18',
    'mull-runner-20',
    'mull-runner',
    required: true,
  )
  mull_plugin = get_option('mull_pass_plugin')
  if mull_plugin == ''
    # Derive /usr/lib/mull-ir-frontend-N from mull-runner-N (override with option).
    runner_base = fs.name(mull_runner.full_path())
    ver_suffix = runner_base.replace('mull-runner-', '').replace('mull-runner', '')
    if ver_suffix != ''
      cand = '/usr/lib/mull-ir-frontend-' + ver_suffix
      if fs.exists(cand)
        mull_plugin = cand
      endif
    endif
  endif
  if mull_plugin == '' or not fs.exists(mull_plugin)
    error(
      'mutation=true: set -Dmull_pass_plugin=/path/to/mull-ir-frontend-N '
      + '(or install Mull so /usr/lib/mull-ir-frontend-N exists)',
    )
  endif
  mull_c_args = [
    '-fpass-plugin=' + mull_plugin,
    '-g',
    '-grecord-command-line',
    '-O0',
  ]
  message('Mull mutation: runner=@0@ plugin=@1@'.format(
    mull_runner.full_path(),
    mull_plugin,
  ))
endif

# policy.capnp imports util.capnp — gen script emits both C pairs into build/.
schema_gen = custom_target(
  'policy_capnp',
  input: files('schema/policy.capnp', 'schema/util.capnp'),
  output: ['policy.capnp.c', 'policy.capnp.h', 'util.capnp.c', 'util.capnp.h'],
  command: [
    gen_capnp,
    '@INPUT0@',
    '@OUTPUT0@',
    '@OUTPUT1@',
    capnpc_c,
  ],
  build_by_default: true,
)

schema_inc = include_directories('.')
janet_inc = include_directories('third_party/janet')

# External HaoZeke/capnp-janet: pkg-config (rg-forge) with wrap fallback.
# Do not vendor that tree here. Subproject supplies janet_mod_src for the host
# to compile against third_party/janet amalgamation.
capnp_janet_dep = dependency(
  'capnp-janet',
  fallback: ['capnp-janet', 'capnp_janet_dep'],
)
capnp_janet_proj = subproject('capnp-janet')
capnp_janet_mod_src = capnp_janet_proj.get_variable('capnp_janet_mod_src')

lib_srcs = files(
  'src/paths.c',
  'src/action_log.c',
  'src/cgroup.c',
  'src/policy.c',
  'src/policy_reason.c',
  'src/policy_janet.c',
  'src/supervisor.c',
  'src/version.c',
  'src/capnp_api.c',
) + [schema_gen] + capnp_janet_mod_src

# Host-owned Janet amalgamation (pack VM). capnp-janet stays external.
janet_amalg = static_library(
  'janet_amalg',
  files('third_party/janet/janet.c'),
  include_directories: janet_inc,
  c_args: [
    '-Wno-error',
    '-Wno-implicit-function-declaration',
    '-Wno-unused-parameter',
    '-Wno-sign-compare',
    '-DJANET_API=',
    '-DJANET_NO_PROCESSES',
    '-DJANET_SPAWN_NO_CHDIR',
  ],
  pic: true,
)

m_dep = cc.find_library('m', required: true)
dl_dep = cc.find_library('dl', required: true)
thread_dep = dependency('threads')

# Default-hidden visibility: only GROK_POLICYD_API symbols leave the .so.
# Tests link the static archive so they can exercise internal helpers without
# exporting those helpers to embedders.
policyd_lib = library(
  'grok_policyd',
  lib_srcs,
  include_directories: [inc, schema_inc, janet_inc],
  dependencies: [capn_c_dep, capnp_janet_dep, m_dep, dl_dep, thread_dep],
  link_with: janet_amalg,
  version: ver,
  soversion: soversion,
  install: true,
  gnu_symbol_visibility: 'hidden',
  c_args: mull_c_args + [
    '-DJANET_API=',
  ],
)

install_headers(
  'include/grok-policyd/supervisor.h',
  subdir: 'grok-policyd',
)

# Install schema SoT for embedders / code generation.
install_data('schema/policy.capnp', install_dir: get_option('datadir') / 'grok-policyd')
install_data('schema/util.capnp', install_dir: get_option('datadir') / 'grok-policyd')

pkg.generate(
  policyd_lib,
  name: 'grok-policyd',
  description: 'GrokOS multi-agent policy TCB (Cap\'n FFI + C ABI)',
  url: 'https://nova.teachx.ai/trace-analysis/grokos-packages/grok-policyd',
  filebase: 'grok-policyd',
  subdirs: '.',
  requires: 'c-capnproto, capnp-janet',
)

cli = executable(
  'grok-policyd',
  files('src/grok-policyd.c'),
  include_directories: inc,
  link_with: policyd_lib,
  dependencies: [capn_c_dep],
  install: true,
)

example = executable(
  'example_minimal',
  files('examples/c/minimal.c'),
  include_directories: inc,
  link_with: policyd_lib,
  dependencies: [capn_c_dep],
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
    'tests/test_pack_lib.c',
    'tests/test_shell_pack.c',
    'tests/test_version.c',
    'tests/test_capnp_ffi.c',
    'tests/test_main.c',
  ),
  include_directories: [inc, schema_inc, janet_inc],
  # Static so tests can use a few internal helpers without exporting them on the .so.
  link_with: [policyd_lib.get_static_lib(), janet_amalg],
  dependencies: [cmocka_dep, capn_c_dep, capnp_janet_dep, m_dep, dl_dep, thread_dep],
  c_args: ['-Wno-format-truncation', '-DJANET_API='] + mull_c_args,
  install: false,
)

test(
  'cmocka',
  supervisor_test,
  protocol: 'exitcode',
  timeout: 120,
  workdir: meson.project_source_root(),
  env: {
    'POLICYD_BUILD_DIR': meson.project_build_root(),
    'POLICYD_SOURCE_ROOT': meson.project_source_root(),
  },
)

# Product link-line for the shipped .so (export + Cap'n runtime + no peer stack).
bash = find_program('bash', required: true)
test(
  'shared_link',
  bash,
  args: [meson.project_source_root() / 'scripts/check_shared_link.sh'],
  env: {'POLICYD_BUILD_DIR': meson.project_build_root()},
  depends: [policyd_lib],
  workdir: meson.project_source_root(),
)

if get_option('mutation')
  # Optional mutation signal on policy/Cap'n. Soft on survivors until Cap'n
  # fail-closed asserts own more of the kill set.
  mull_report_dir = meson.current_build_dir() / 'mull-report'
  run_target(
    'mutation',
    command: [
      mull_runner,
      '--timeout', '15000',
      '--allow-surviving',
      '--report-name', 'grok-policyd',
      '--report-dir', mull_report_dir,
      '--reporters', 'Elements',
      '--reporters', 'IDE',
      supervisor_test,
    ],
    depends: [supervisor_test],
    env: {
      'MULL_CONFIG': meson.project_source_root() / 'mull.yml',
      'POLICYD_BUILD_DIR': meson.project_build_root(),
      # Shell pack tests resolve policy/shell.janet via POLICYD_SOURCE_ROOT.
      'POLICYD_SOURCE_ROOT': meson.project_source_root(),
    },
  )
endif

alias_target('lib', policyd_lib)
alias_target('cli', cli)
alias_target('example', example)
alias_target('tests', supervisor_test)
