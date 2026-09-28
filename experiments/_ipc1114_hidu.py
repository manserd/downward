#! /usr/bin/env python

import os

import custom_parser
import project

REPO = project.get_repo_base() # path to downward repo
BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"] # path to benchmarks
SCP_LOGIN = "myname@myserver.com"
REMOTE_REPOS_DIR = "/infai/username/projects"
REVISION_CACHE = os.environ.get("DOWNWARD_REVISION_CACHE") # path to revision cache
ENV = project.BaselSlurmEnvironment(email="my.name@myhost.ch") # TODO: email?
SUITE_11 = ["barman-opt11-strips", "barman-sat11-strips", "elevators-opt11-strips", "elevators-sat11-strips", "floortile-opt11-strips", "floortile-sat11-strips", "nomystery-opt11-strips", "nomystery-sat11-strips", "openstacks-opt11-strips", "openstacks-sat11-strips", "parcprinter-opt11-strips", "parcprinter-sat11-strips", "parking-opt11-strips", "parking-sat11-strips", "pegsol-opt11-strips", "pegsol-sat11-strips", "scanalyzer-opt11-strips", "scanalyzer-sat11-strips", "sokoban-opt11-strips", "sokoban-sat11-strips", "tidybot-opt11-strips", "tidybot-sat11-strips", "transport-opt11-strips", "transport-sat11-strips", "visitall-opt11-strips", "visitall-sat11-strips", "woodworking-opt11-strips", "woodworking-sat11-strips"]
SUITE_14 = ["barman-mco14-strips", "barman-opt14-strips", "barman-sat14-strips", "cavediving-14-adl", "childsnack-opt14-strips", "childsnack-sat14-strips", "citycar-opt14-adl", "citycar-sat14-adl", "floortile-opt14-strips", "floortile-sat14-strips", "ged-opt14-strips", "ged-sat14-strips", "hiking-agl14-strips", "hiking-opt14-strips", "hiking-sat14-strips", "maintenance-opt14-adl", "maintenance-sat14-adl", "openstacks-agl14-strips", "openstacks-opt14-strips", "openstacks-sat14-strips", "parking-opt14-strips", "parking-sat14-strips", "tetris-opt14-strips", "tetris-sat14-strips", "thoughtful-mco14-strips", "thoughtful-sat14-strips", "tidybot-opt14-strips", "transport-opt14-strips", "transport-sat14-strips", "visitall-opt14-strips", "visitall-sat14-strips"]
SUITE = SUITE_11 + SUITE_14

CONFIGS = [
  ('gbfs', ['--search', 'eager(single(ff()))']),
]
for bs in [
  # 'U',
  # 'H'
]:
  CONFIGS.append((f'hg{bs}u', ['--search', f'let(h, ff(), eager(alt([single(h), one(type_system="hg", evaluators=[h, g()], heuristic=h, bucket_selection="{bs}", bucket_temperature=1.0, state_selection="U", state_temperature=1.0)])))']))
for ts in [
  'hi',
  # 'lw'
]:
  for bs in [
    # 'U',
    # 'H',
    'D'
  ]:
    for ss in [
      'U',
      # 'H'
    ]:
      CONFIGS.append((f'{ts}{bs}{ss.lower()}', ['--search', f'let(h, ff(), eager(alt([single(h), one(type_system="{ts}", evaluators=[], heuristic=h, bucket_selection="{bs}", bucket_temperature=1.0, state_selection="{ss}", state_temperature=1.0)])))']))

BUILD_OPTIONS = []
DRIVER_OPTIONS = ["--overall-time-limit", "30m"] # time limit!

# repo revision axis
REV_NICKS = [
  ("main", ""),
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
  project.EVALUATIONS_PER_TIME,
]

exp = project.FastDownwardExperiment(environment=ENV, revision_cache=REVISION_CACHE)
for config_nick, config in CONFIGS:
  for rev, rev_nick in REV_NICKS:
    algo_name = f"{rev_nick}:{config_nick}" if rev_nick else config_nick # probably just a descriptive name
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

# probably irrelevant for me
if not project.REMOTE:
  project.add_scp_step(exp, SCP_LOGIN, REMOTE_REPOS_DIR)

# TODO: ?
attributes = ["expansions"]
pairs = [
  ("01-cg", "02-ff"),
]
suffix = "-rel" if project.RELATIVE else ""
for algo1, algo2 in pairs:
  for attr in attributes:
    exp.add_report(
        project.ScatterPlotReport(
            relative=project.RELATIVE,
            get_category=None if project.TEX else lambda run1, run2: run1["domain"],
            attributes=[attr],
            filter_algorithm=[algo1, algo2],
            filter=[project.add_evaluations_per_time],
            format="tex" if project.TEX else "png",
        ),
        name=f"{exp.name}-{algo1}-vs-{algo2}-{attr}{suffix}",
    )

project.add_compress_exp_dir_step(exp)

exp.run_steps()
