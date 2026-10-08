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

SUITE = [
  "barman-sat14-strips",
  "childsnack-sat14-strips", # consistently 0/20
  "elevators-sat11-strips",
  "floortile-sat11-strips",
  "ged-sat14-strips", # consistently about 18/20
  "hiking-sat14-strips", # consistently about 20/20
  "nomystery-sat11-strips",
  "openstacks-sat14-strips",
  "parcprinter-sat11-strips", # consistently about 20/20
  "parking-sat11-strips",
  "pegsol-sat11-strips", # consistently about 20/20
  "scanalyzer-sat11-strips", # consistently about 19/20
  "sokoban-sat11-strips", # consistently about 19/20
  "tetris-sat14-strips",
  "thoughtful-sat14-strips",
  "tidybot-sat11-strips",
  "transport-sat11-strips",
  "visitall-sat14-strips",
  "woodworking-sat11-strips",
]

COST_TYPE = 'one'
H_EXPR = f'ff(transform=adapt_costs(cost_type={COST_TYPE}))'

def searchex(ts, evals, bs, bt, ss, st, seed):
  return f'let(h, {H_EXPR}, eager(alt([single(h), one(' \
    f'type_system="{ts}", evaluators=[{evals}], heuristic=h, ' \
    f'bucket_selection="{bs}", bucket_temperature={str(float(bt))}, ' \
    f'state_selection="{ss}", state_temperature={str(float(st))}, '\
    f'random_seed={seed})]), cost_type={COST_TYPE}))'

CONFIGS = [
  # ('gbfs', ['--search', f'eager(single({H_EXPR}), cost_type={COST_TYPE})']),
]
for bt in [0.0625, 0.125, 0.25, 0.5]:
  bt_ = str(bt).replace('.', '_')
  for st in [1]:
    st_ = str(st).replace('.', '_')
    for seed in [0, 1, 2, 3, 4]:
      for bs in [
        # 'U',
        # 'H'
      ]:
        CONFIGS.append((f'hg-{bs}{bt_}-u{st_}-{seed}', ['--search', searchex('hg', 'h, g()', bs, bt, 'U', st, seed)]))
      for ts in [
        # 'hi',
        'lw'
      ]:
        for bs in [
          # 'U',
          'H',
          'D'
        ]:
          for ss in [
            'U',
            'H'
          ]:
            CONFIGS.append((f'{ts}-{bs}{bt_}-{ss.lower()}{st_}-{seed}', ['--search', searchex(ts, '', bs, bt, ss, st, seed)]))

BUILD_OPTIONS = []
DRIVER_OPTIONS = [
  "--overall-time-limit", "10m",
  "--overall-memory-limit", "3584M" # default/max 3872MiB (Cluster instructions for student.pdf)
]

# repo revision axis
REV_NICKS = [
  ("expfix", ""),
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
