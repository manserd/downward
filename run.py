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

SEED = os.environ['SEED'] if 'SEED' in os.environ else 2

SEARCHES = {
    'astar': 'astar(ff())',
    'gbfs': 'eager(single(ff()))',
    'type': f'let(h, ff(), eager(alt([single(h), type_based([h, g()], random_seed={SEED})])))',
    'hieval': f'let(h, ff(), eager(alt([single(h), type_based([hi(h)], random_seed={SEED})])))',
    'hiol': 'let(h, ff(), eager(alt([single(h), lw_list(h)])))',
    'lweval': f'let(h, ff(), eager(alt([single(h), type_based([lw(h)], random_seed={SEED})])))',
    'lwol': 'let(h, ff(), eager(alt([single(h), hi_list(progress(h))])))',
    'onetype': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"hg\\", evaluators=[h, g()], heuristic=h, bucket_selection=\\"U\\", bucket_temperature=1.0, state_selection=\\"U\\", state_temperature=1.0)])))',
    'hgUu': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"hg\\", evaluators=[h, g()], heuristic=h, bucket_selection=\\"U\\", bucket_temperature=1.0, state_selection=\\"U\\", state_temperature=1.0)])))',
    'hgHu': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"hg\\", evaluators=[h, g()], heuristic=h, bucket_selection=\\"H\\", bucket_temperature=1.0, state_selection=\\"U\\", state_temperature=1.0)])))',
    'hiUu': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"hi\\", evaluators=[], heuristic=h, bucket_selection=\\"U\\", bucket_temperature=1.0, state_selection=\\"U\\", state_temperature=1.0)])))',
    'hiUh': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"hi\\", evaluators=[], heuristic=h, bucket_selection=\\"U\\", bucket_temperature=1.0, state_selection=\\"H\\", state_temperature=1.0)])))',
    'hiHu': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"hi\\", evaluators=[], heuristic=h, bucket_selection=\\"H\\", bucket_temperature=1.0, state_selection=\\"U\\", state_temperature=1.0)])))',
    'hiHh': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"hi\\", evaluators=[], heuristic=h, bucket_selection=\\"H\\", bucket_temperature=1.0, state_selection=\\"H\\", state_temperature=1.0)])))',
    'hiDu': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"hi\\", evaluators=[], heuristic=h, bucket_selection=\\"D\\", bucket_temperature=1.0, state_selection=\\"U\\", state_temperature=1.0)])))',
    'hiDh': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"hi\\", evaluators=[], heuristic=h, bucket_selection=\\"D\\", bucket_temperature=1.0, state_selection=\\"H\\", state_temperature=1.0)])))',
    'lwUu': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"lw\\", evaluators=[], heuristic=h, bucket_selection=\\"U\\", bucket_temperature=1.0, state_selection=\\"U\\", state_temperature=1.0)])))',
    'lwUh': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"lw\\", evaluators=[], heuristic=h, bucket_selection=\\"U\\", bucket_temperature=1.0, state_selection=\\"H\\", state_temperature=1.0)])))',
    'lwHu': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"lw\\", evaluators=[], heuristic=h, bucket_selection=\\"H\\", bucket_temperature=1.0, state_selection=\\"U\\", state_temperature=1.0)])))',
    'lwHh': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"lw\\", evaluators=[], heuristic=h, bucket_selection=\\"H\\", bucket_temperature=1.0, state_selection=\\"H\\", state_temperature=1.0)])))',
    'lwDu': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"lw\\", evaluators=[], heuristic=h, bucket_selection=\\"D\\", bucket_temperature=1.0, state_selection=\\"U\\", state_temperature=1.0)])))',
    'lwDh': 'let(h, ff(), eager(alt([single(h), one(type_system=\\"lw\\", evaluators=[], heuristic=h, bucket_selection=\\"D\\", bucket_temperature=1.0, state_selection=\\"H\\", state_temperature=1.0)])))',
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

# "note that options are passed without --, e.g., python3 build.py build=debug" (https://github.com/aibasel/downward/blob/main/BUILD.md#optional-plan-validator)
# I haven't had any problems so far, but maybe this will matter at some point.
if not mode[0] == '_':
    build_command = 'python3 build.py'
    if mode[-1] == 'd':
        build_command += ' --debug'
    print(build_command)
    subprocess.run(build_command, shell=True, check=True)

if not mode[-1] == '_':
  run_command = 'python3 fast-downward.py'
  if mode[0] == 'd':
      run_command += ' --build debug'
  run_command += f' --validate "{BENCHMARKS[benchmark]}" --search "{SEARCHES[search]}"'
  print(run_command)
  subprocess.run(run_command, shell=True)
