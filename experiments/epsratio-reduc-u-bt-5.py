#! /usr/bin/env python

import os

import project

import experiments.old.custom_parser as custom_parser

REPO = project.get_repo_base() # path to downward repo
BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"] # path to benchmarks
SCP_LOGIN = "myname@myserver.com"
REMOTE_REPOS_DIR = "/infai/username/projects"
REVISION_CACHE = os.environ.get("DOWNWARD_REVISION_CACHE") # path to revision cache
ENV = project.BaselSlurmEnvironment(email="my.name@myhost.ch")

# tomasz
SUITE_REFERENCE = [
  "barman-sat14-strips",
  "childsnack-sat14-strips",
  "elevators-sat11-strips",
  "floortile-sat11-strips",
  "ged-sat14-strips",
  "hiking-sat14-strips",
  "nomystery-sat11-strips",
  "openstacks-sat14-strips",
  "parcprinter-sat11-strips",
  "parking-sat11-strips",
  "pegsol-sat11-strips",
  "scanalyzer-sat11-strips",
  "sokoban-sat11-strips",
  "tetris-sat14-strips",
  "thoughtful-sat14-strips",
  "tidybot-sat11-strips",
  "transport-sat11-strips",
  "visitall-sat14-strips",
  "woodworking-sat11-strips",
]

SUITE_REDUCED = [
  "barman-sat14-strips",
  # "childsnack-sat14-strips", # consistently 0/20
  "elevators-sat11-strips",
  "floortile-sat11-strips",
  # "ged-sat14-strips", # consistently about 18/20
  # "hiking-sat14-strips", # consistently about 20/20
  "nomystery-sat11-strips",
  "openstacks-sat14-strips",
  # "parcprinter-sat11-strips", # consistently about 20/20
  "parking-sat11-strips",
  # "pegsol-sat11-strips", # consistently about 20/20
  # "scanalyzer-sat11-strips", # consistently about 19/20
  # "sokoban-sat11-strips", # consistently about 19/20
  "tetris-sat14-strips",
  "thoughtful-sat14-strips",
  "tidybot-sat11-strips",
  "transport-sat11-strips",
  "visitall-sat14-strips",
  "woodworking-sat11-strips",
]

# ipc 11-23, duplicates removed
SUITE_LARGE = [
  "agricola-sat18-strips",
  "barman-sat14-strips",
  "caldera-sat18-adl",
  "caldera-split-sat18-adl",
  "cavediving-14-adl",
  "childsnack-sat14-strips",
  "citycar-sat14-adl",
  "data-network-sat18-strips",
  "elevators-sat11-strips",
  "flashfill-sat18-adl",
  "floortile-sat14-strips",
  "folding-sat23-adl",
  "ged-sat14-strips",
  "hiking-sat14-strips",
  "labyrinth-sat23-adl",
  "maintenance-sat14-adl",
  "nomystery-sat11-strips",
  "nurikabe-sat18-adl",
  "openstacks-sat14-strips",
  "organic-synthesis-sat18-strips",
  "organic-synthesis-split-sat18-strips",
  "parcprinter-sat11-strips",
  "parking-sat14-strips",
  "pegsol-sat11-strips",
  "quantum-layout-sat23-strips",
  "recharging-robots-sat23-adl",
  "ricochet-robots-sat23-adl",
  "rubiks-cube-sat23-adl",
  "scanalyzer-sat11-strips",
  "settlers-sat18-adl",
  "slitherlink-sat23-adl",
  "snake-sat18-strips",
  "sokoban-sat11-strips",
  "spider-sat18-strips",
  "termes-sat18-strips",
  "tetris-sat14-strips",
  "thoughtful-sat14-strips",
  "tidybot-sat11-strips",
  "transport-sat14-strips",
  "visitall-sat14-strips",
  "woodworking-sat11-strips",
]

SUITE = SUITE_REDUCED

COST_TYPE = 'one'
H_EXPR = f'ff(transform=adapt_costs(cost_type={COST_TYPE}))'

def oneex(ts, evals, bs, bt, ss, st, seed):
  return f'let(h, {H_EXPR}, eager(alt([single(h), one(' \
    f'type_system="{ts}", evaluators=[{evals}], heuristic=h, ' \
    f'bucket_selection="{bs}", bucket_temperature={str(float(bt))}, ' \
    f'state_selection="{ss}", state_temperature={str(float(st))}, '\
    f'random_seed={seed})]), cost_type={COST_TYPE}))'

def epsex(ts, evals, bs, bt, ss, st, seed, epsilon):
  return f'let(h, {H_EXPR}, eager(eps(single(h), one(' \
    f'type_system="{ts}", evaluators=[{evals}], heuristic=h, ' \
    f'bucket_selection="{bs}", bucket_temperature={str(float(bt))}, ' \
    f'state_selection="{ss}", state_temperature={str(float(st))}, '\
    f'random_seed={seed}), epsilon={epsilon}, random_seed={seed + 1000}), cost_type={COST_TYPE}))'

def onecfg(ts, bs, bt, ss, st, seed):
  evals = 'h, g()' if ts == 'hg' else ''
  bt_ = str(bt).replace('.', '_')
  st_ = str(st).replace('.', '_')
  return (f'{ts}-{bs.upper()}{bt_}-{ss.lower()}{st_}-{seed}', ['--search', oneex(ts, evals, bs.upper(), bt, ss.upper(), st, seed)])

def epscfg(ts, bs, bt, ss, st, seed, epsilon):
  evals = 'h, g()' if ts == 'hg' else ''
  bt_ = str(bt).replace('.', '_')
  st_ = str(st).replace('.', '_')
  epsilon_ = str(epsilon).replace('.', '_')
  return (f'{ts}{epsilon_}-{bs.upper()}{bt_}-{ss.lower()}{st_}-{seed}', ['--search', epsex(ts, evals, bs.upper(), bt, ss.upper(), st, seed, epsilon)])

CONFIGS = [
  # ('gbfs', ['--search', f'eager(single({H_EXPR}), cost_type={COST_TYPE})']),
]
# for bt in [1]:
for bt in [0.25, 0.5, 2, 5, 10]:
  for st in [1]:
  # for st in [0.25, 0.5, 2, 5, 10]:
    for seed in [0, 1, 2, 3, 4]:
      # 14:
      # CONFIGS.append(onecfg('hg', 'U', bt, 'u', st, seed))
      CONFIGS.append(onecfg('hg', 'H', bt, 'u', st, seed))
      # CONFIGS.append(onecfg('hi', 'U', bt, 'u', st, seed))
      # CONFIGS.append(onecfg('hi', 'U', bt, 'h', st, seed))
      CONFIGS.append(onecfg('hi', 'H', bt, 'u', st, seed))
      CONFIGS.append(onecfg('hi', 'H', bt, 'h', st, seed))
      CONFIGS.append(onecfg('hi', 'D', bt, 'u', st, seed))
      CONFIGS.append(onecfg('hi', 'D', bt, 'h', st, seed))
      # CONFIGS.append(onecfg('lw', 'U', bt, 'u', st, seed))
      # CONFIGS.append(onecfg('lw', 'U', bt, 'h', st, seed))
      CONFIGS.append(onecfg('lw', 'H', bt, 'u', st, seed))
      CONFIGS.append(onecfg('lw', 'H', bt, 'h', st, seed))
      CONFIGS.append(onecfg('lw', 'D', bt, 'u', st, seed))
      CONFIGS.append(onecfg('lw', 'D', bt, 'h', st, seed))
      # for epsilon in [0.5]:
      # for epsilon in [0.1, 0.25, 0.5, 0.75]:
        # 14:
        # CONFIGS.append(epscfg('hg', 'U', bt, 'u', st, seed, epsilon))
        # CONFIGS.append(epscfg('hg', 'H', bt, 'u', st, seed, epsilon))
        # CONFIGS.append(epscfg('hi', 'U', bt, 'u', st, seed, epsilon))
        # CONFIGS.append(epscfg('hi', 'U', bt, 'h', st, seed, epsilon))
        # CONFIGS.append(epscfg('hi', 'H', bt, 'u', st, seed, epsilon))
        # CONFIGS.append(epscfg('hi', 'H', bt, 'h', st, seed, epsilon))
        # CONFIGS.append(epscfg('hi', 'D', bt, 'u', st, seed, epsilon))
        # CONFIGS.append(epscfg('hi', 'D', bt, 'h', st, seed, epsilon))
        # CONFIGS.append(epscfg('lw', 'U', bt, 'u', st, seed, epsilon))
        # CONFIGS.append(epscfg('lw', 'U', bt, 'h', st, seed, epsilon))
        # CONFIGS.append(epscfg('lw', 'H', bt, 'u', st, seed, epsilon))
        # CONFIGS.append(epscfg('lw', 'H', bt, 'h', st, seed, epsilon))
        # CONFIGS.append(epscfg('lw', 'D', bt, 'u', st, seed, epsilon))
        # CONFIGS.append(epscfg('lw', 'D', bt, 'h', st, seed, epsilon))

BUILD_OPTIONS = []
DRIVER_OPTIONS = [
  "--overall-time-limit", "10m",
  "--overall-memory-limit", "3584M" # default/max 3872MiB (Cluster instructions for student.pdf)
]

# repo revision axis
REV_NICKS = [
  ("epsratio", ""),
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
  project.EXPLORATION_RATIO,
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
