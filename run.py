import os
import platform
import subprocess
import sys

assert os.path.isdir('builds')
# assert (os.path.isdir('builds_windows') and not os.path.isdir('builds_macos')) or (
    # os.path.isdir('builds_macos') and not os.path.isdir('builds_windows'))

if platform.system() == 'Windows':
    if os.path.isdir('builds_macos'):
        print('Already using Windows builds')
    else:
        print('Switching to Windows builds')
        os.rename('builds', 'builds_macos')
        os.rename('builds_windows', 'builds')
elif platform.system() == 'Darwin':
    if os.path.isdir('builds_windows'):
        print('Already using macOS builds')
    else:
        print('Switching to macOS builds')
        os.rename('builds', 'builds_windows')
        os.rename('builds_macos', 'builds')
    # macOS requires manually adding the working directory to PATH (for ./validate).
    os.environ['PATH'] += ':.'

COST_TYPE = 'one'
H_EXPR = f'ff(transform=adapt_costs(cost_type={COST_TYPE}))'
SEED = os.environ['SEED'] if 'SEED' in os.environ else 0

def one(ts, evals, bs, bt, ss, st, seed):
    return f'let(h, {H_EXPR}, eager(alt([single(h), one(type_system=\\"{ts}\\", evaluators=[{evals}], heuristic=h, bucket_selection=\\"{bs}\\", bucket_temperature={bt}, state_selection=\\"{ss}\\", state_temperature={st}, random_seed={seed})]), cost_type={COST_TYPE}))'

SEARCHES = {
    'astar': f'astar({H_EXPR})',
    'gbfs': f'eager(single({H_EXPR}), cost_type={COST_TYPE})',
    'type': f'let(h, {H_EXPR}, eager(alt([single(h), type_based([h, g()], random_seed={SEED})]), cost_type={COST_TYPE}))',
}
for bs in ['U', 'H']:
    SEARCHES[f'hg{bs}u'] = one('hg', 'h, g()', bs, '1.0', 'U', '1.0', SEED)
for ts in ['hi', 'lw']:
    for bs in ['U', 'H', 'D']:
        for ss in ['U', 'H']:
            SEARCHES[f'{ts}{bs}{ss.lower()}'] = one(ts, '', bs, '1.0', ss, '1.0', SEED)

BENCHMARKS = {
    'gripper': 'misc/tests/benchmarks/gripper/prob01.pddl',
    'miconic': 'misc/tests/benchmarks/miconic/s1-0.pddl',
    'miconic2': 'misc/tests/benchmarks/miconic-simpleadl/s1-0.pddl',
    'phil': 'misc/tests/benchmarks/philosophers/p01-phil2.pddl',
    'satellite': 'misc/tests/benchmarks/satellite/p25-HC-pfile5.pddl',
    'ticket': 'examples/ticket/1.pddl',
    'ticket2': 'examples/ticket2/1.pddl',
    'redirect': 'examples/redirect/1.pddl',
    'redirect2': 'examples/redirect2/1.pddl',
}

MODES = ['r', '_r', 'r_', 'd', '_d', 'd_']

mode = sys.argv[1]
if not mode in MODES:
    print('Bad mode. Use one of:')
    print('\n'.join(map(lambda i: '- ' + i, MODES)))
    exit(1)

search = sys.argv[2]
if not search in SEARCHES:
    print('Unknown search. Use one of:')
    print('\n'.join(map(lambda i: '- ' + i, SEARCHES.keys())))
    exit(1)

benchmark = sys.argv[3]
if not benchmark in BENCHMARKS:
    print('Unknown benchmark. Use one of:')
    print('\n'.join(map(lambda i: '- ' + i, BENCHMARKS.keys())))
    exit(1)

debug = 'd' in mode
do_build = mode[0] != '_'
do_run = mode[-1] != '_'

# "note that options are passed without --, e.g., python3 build.py build=debug" (https://github.com/aibasel/downward/blob/main/BUILD.md#optional-plan-validator)
# I haven't had any problems so far, but maybe this will matter at some point.
if do_build:
    build_command = 'python3 build.py'
    if debug:
        build_command += ' --debug'
    print(build_command)
    subprocess.run(build_command, shell=True, check=True)

if do_run:
  run_command = 'python3 fast-downward.py'
  if debug:
      run_command += ' --build debug'
  run_command += f' --validate "{BENCHMARKS[benchmark]}" --search "{SEARCHES[search]}"'
  print(run_command)
  subprocess.run(run_command, shell=True)
