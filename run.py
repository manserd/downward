import os
import platform
import subprocess
import sys

assert os.path.isdir('builds')
assert (os.path.isdir('builds_windows') and not os.path.isdir('builds_macos')) or (
    os.path.isdir('builds_macos') and not os.path.isdir('builds_windows'))

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

SEED = 2

SEARCHES = {
    'astar': 'astar(ff())',
    'gbfs': 'eager(single(ff()))',
    'type': f'let(h, ff(), eager(alt([single(h), type_based([h, g()], random_seed={SEED})])))',
    '_hi': f'eager(type_based([hi(ff())], random_seed={SEED}))',
    'hi': f'let(h, ff(), eager(alt([single(h), type_based([hi(h)], random_seed={SEED})])))',
    'hiol': 'let(h, ff(), eager(alt([single(h), lw_list(h)])))',
    '_lw': f'eager(type_based([lw(ff())], random_seed={SEED}))',
    'lw': f'let(h, ff(), eager(alt([single(h), type_based([lw(h)], random_seed={SEED})])))',
    'lwol': 'let(h, ff(), eager(alt([single(h), hi_list(progress(h))])))',
}

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

MODES = ['r', 'r_', 'd', 'd_']

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

# "note that options are passed without --, e.g., python3 build.py build=debug" (https://github.com/aibasel/downward/blob/main/BUILD.md#optional-plan-validator)
# I haven't had any problems so far, but maybe this will matter at some point.
if not mode[-1] == '_':
    build_command = 'python3 build.py'
    if mode[0] == 'd':
        build_command += ' --debug'
    print(build_command)
    subprocess.run(build_command, shell=True, check=True)

run_command = 'python3 fast-downward.py'
if mode[0] == 'd':
    run_command += ' --build debug'
run_command += f' --validate "{BENCHMARKS[benchmark]}" --search "{SEARCHES[search]}"'
print(run_command)
subprocess.run(run_command, shell=True)
