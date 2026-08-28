import subprocess
import re

for i in range(-1, 1000):
  result = subprocess.run(
      f'python3 fast-downward.py "examples/ticket2/1.pddl" --search "let(h, ff(), eager(alt([single(h), type_based([hi(h)], random_seed={i})])))"',
      shell=True,
      check=True,
      capture_output=True,
      text=True
  )
  # print(result.stdout)
  match = re.search(r'^\[t=\d+\.\d+s, \d+ KB\] Expanded (\d+) state\(s\)\.$', result.stdout, flags=re.MULTILINE)
  print(i, match.group(1))
  # assert match.group(1) == '1'
