#! /usr/bin/env python

import os

import custom_parser
import project

REPO = project.get_repo_base() # path to downward repo
BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"] # path to benchmarks
SCP_LOGIN = "myname@myserver.com"
REMOTE_REPOS_DIR = "/infai/username/projects"
REVISION_CACHE = os.environ.get("DOWNWARD_REVISION_CACHE") # path to revision cache
ENV = project.BaselSlurmEnvironment(email="my.name@myhost.ch")

# "We used benchmark problems from the satisficing tracks of IPC 2011, 2014, [~~and 2018~~].
# In domains that were included in both 2011 and 2014 sets, the 2014 version was taken"
# This statement appears to be inaccurate. The following matches table 2 from the paper:
SUITE_SAT1114 = ["barman-sat14-strips", "childsnack-sat14-strips", "elevators-sat11-strips", "floortile-sat11-strips", "ged-sat14-strips", "hiking-sat14-strips", "nomystery-sat11-strips", "openstacks-sat14-strips", "parcprinter-sat11-strips", "parking-sat11-strips", "pegsol-sat11-strips", "scanalyzer-sat11-strips", "sokoban-sat11-strips", "tetris-sat14-strips", "thoughtful-sat14-strips", "tidybot-sat11-strips", "transport-sat11-strips", "visitall-sat14-strips", "woodworking-sat11-strips"]

SUITE = SUITE_SAT1114

COST_TYPE = 'one'
H_EXPR = f'ff(transform=adapt_costs(cost_type={COST_TYPE}))'

def searchex(ts, evals, bs, bt, ss, st, seed):
    return f'let(h, {H_EXPR}, eager(alt([single(h), one(type_system="{ts}", evaluators=[{evals}], heuristic=h, bucket_selection="{bs}", bucket_temperature={bt}, state_selection="{ss}", state_temperature={st}, random_seed={seed})]), cost_type={COST_TYPE}))'

CONFIGS = [
  ('gbfs', ['--search', f'eager(single({H_EXPR}), cost_type={COST_TYPE})']),
]
for seed in [0, 1, 2, 3, 4]:
  for bs in [
    'U',
    'H'
  ]:
    CONFIGS.append((f'hg{bs}u-{seed}', ['--search', searchex('hg', 'h, g()', bs, '1.0', 'U', '1.0', seed)]))
  for ts in [
    'hi',
    'lw'
  ]:
    for bs in [
      'U',
      'H',
      'D'
    ]:
      for ss in [
        'U',
        'H'
      ]:
        CONFIGS.append((f'{ts}{bs}{ss.lower()}-{seed}', ['--search', searchex(ts, '', bs, '1.0', ss, '1.0', seed)]))

# sat1114 all once: (1+2+2*3*2)*380=5700
# sat1114 all 5x: (1+5*(2+2*3*2))*380=26980

BUILD_OPTIONS = []
DRIVER_OPTIONS = [
  "--overall-time-limit", "10m",
  "--overall-memory-limit", "3584M" # default/max 3872MiB (Cluster instructions for student.pdf)
]

# repo revision axis
REV_NICKS = [
  ("nocachew", ""),
]

ATTRIBUTES = [
  "error",
  "run_dir",
  "search_start_time",
  "search_start_memory",
  "total_time",
  "h_values",
  "coverage",
  "expansions",
  "memory",
  "cost",
  "plan_length",
  project.EVALUATIONS_PER_TIME,
]

exp = project.FastDownwardExperiment(environment=ENV, revision_cache=REVISION_CACHE)
for config_nick, config in CONFIGS:
  for rev, rev_nick in REV_NICKS:
    algo_name = f"{rev_nick}:{config_nick}" if rev_nick else config_nick
    exp.add_algorithm(
        algo_name,
        REPO,
        rev,
        config,
        build_options=BUILD_OPTIONS,
        driver_options=DRIVER_OPTIONS,
    )
exp.add_suite(BENCHMARKS_DIR, SUITE)

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(custom_parser.get_parser()) # example for a custom parser
exp.add_parser(exp.PLANNER_PARSER)

exp.add_step("build", exp.build)
exp.add_step("start", exp.start_runs)
exp.add_step("parse", exp.parse)
exp.add_fetcher(name="fetch")

project.add_absolute_report(
    exp, attributes=ATTRIBUTES, filter=[project.add_evaluations_per_time]
)

exp.run_steps()
