#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Uso: $0 <benchmark.csv> [label_v1] [label_v2]"
  echo "Esempio: $0 l2_cnf_no_equality_benchmark_results.csv FO2 Classic"
  exit 1
fi

CSV_FILE="$1"
LEFT_LABEL="${2:-FO2}"
RIGHT_LABEL="${3:-Classic}"

if [[ ! -f "$CSV_FILE" ]]; then
  echo "Errore: file non trovato: $CSV_FILE"
  exit 1
fi

python3 - "$CSV_FILE" "$LEFT_LABEL" "$RIGHT_LABEL" <<'PY'
import csv
import math
import sys
from collections import Counter, defaultdict

csv_path, left_label, right_label = sys.argv[1], sys.argv[2], sys.argv[3]

def parse_time(value):
    value = (value or '').strip()
    if value == '':
        return None
    try:
        return float(value)
    except ValueError:
        return None


def status_group(status):
    status = (status or '').strip()
    if status in {'Satisfiable', 'Unsatisfiable'}:
        return 'SOLVED'
    if status == 'Timeout':
        return 'TIMEOUT'
    if status == 'Error':
        return 'ERROR'
    return 'OTHER'


rows = []
with open(csv_path, newline='') as f:
    reader = csv.reader(f)
    for row in reader:
        if not row or all((c.strip() == '') for c in row):
            continue
        first_col = row[0].strip().lower()
        if first_col in {'filename', 'dataset'}:
            continue
        if len(row) >= 6:
            dataset = row[0].strip()
            file_name = f"{dataset}/{row[1].strip()}"
            left_status = row[2].strip()
            left_time = parse_time(row[3])
            right_status = row[4].strip()
            right_time = parse_time(row[5])
        elif len(row) == 5:
            file_name = row[0].strip()
            left_status = row[1].strip()
            left_time = parse_time(row[2])
            right_status = row[3].strip()
            right_time = parse_time(row[4])
        else:
            continue

        rows.append({
            'file': file_name,
            'left_status': left_status,
            'left_time': left_time,
            'right_status': right_status,
            'right_time': right_time,
        })

if not rows:
    print(f"Nessuna riga valida trovata in {csv_path}")
    sys.exit(1)

left_counts = Counter()
right_counts = Counter()
left_solved = Counter()
right_solved = Counter()
status_mismatches = []
status_equal = 0
status_diff = 0
faster_left = 0
faster_right = 0
same_time = 0
both_solved_and_compared = 0

def add_count(counter, status):
    counter[status] += 1

for r in rows:
    l_s = r['left_status']
    r_s = r['right_status']
    l_t = r['left_time']
    r_t = r['right_time']

    add_count(left_counts, l_s)
    add_count(right_counts, r_s)

    if l_s in {'Satisfiable', 'Unsatisfiable'}:
        left_solved[l_s] += 1
    if r_s in {'Satisfiable', 'Unsatisfiable'}:
        right_solved[r_s] += 1

    if l_s == r_s:
        status_equal += 1
    else:
        status_diff += 1
        status_mismatches.append((r['file'], l_s, l_t, r_s, r_t))

    if l_s in {'Satisfiable', 'Unsatisfiable'} and r_s in {'Satisfiable', 'Unsatisfiable'}:
        both_solved_and_compared += 1
        if l_t is not None and r_t is not None:
            if l_t < r_t:
                faster_left += 1
            elif l_t > r_t:
                faster_right += 1
            else:
                same_time += 1

print('=== Confronto benchmark Vampire ===')
print(f'File analizzato: {csv_path}')
print(f'Versione 1: {left_label}')
print(f'Versione 2: {right_label}')
print(f'Righe totali: {len(rows)}')
print()
print(f'{left_label} - status:')
for status in ['Satisfiable', 'Unsatisfiable', 'Timeout', 'Error']:
    print(f'  {status:12} {left_counts.get(status, 0)}')
print(f'  Solved     {sum(left_solved.values())}')
print()
print(f'{right_label} - status:')
for status in ['Satisfiable', 'Unsatisfiable', 'Timeout', 'Error']:
    print(f'  {status:12} {right_counts.get(status, 0)}')
print(f'  Solved     {sum(right_solved.values())}')
print()
print('Status concordanti:', status_equal)
print('Status discordanti:', status_diff)
print('Casi risolti da entrambe le versioni e confrontabili in tempo:', both_solved_and_compared)
print(f'{left_label} più veloce nei casi risolti da entrambe: {faster_left}')
print(f'{right_label} più veloce nei casi risolti da entrambe: {faster_right}')
print(f'Stessi tempi nei casi risolti da entrambe: {same_time}')
print()

if status_mismatches:
    print('=== File con differenze di status ===')
    for file_name, l_s, l_t, r_s, r_t in status_mismatches:
        print(f'{file_name}: {left_label}={l_s}({l_t if l_t is not None else "-"}) | {right_label}={r_s}({r_t if r_t is not None else "-"})')
else:
    print('=== File con differenze di status ===')
    print('Nessuna differenza di status rilevata.')

print()
print('=== Casi in cui una versione batte l\'altra ===')

wins_left = 0
wins_right = 0
left_better = []
right_better = []

for r in rows:
    l_s = r['left_status']
    r_s = r['right_status']
    l_t = r['left_time']
    r_t = r['right_time']

    if l_s in {'Satisfiable', 'Unsatisfiable'} and r_s in {'Timeout', 'Error'}:
        wins_left += 1
        left_better.append((r['file'], l_s, l_t, r_s, r_t))
    elif r_s in {'Satisfiable', 'Unsatisfiable'} and l_s in {'Timeout', 'Error'}:
        wins_right += 1
        right_better.append((r['file'], l_s, l_t, r_s, r_t))
    elif l_s == r_s and l_t is not None and r_t is not None and l_s in {'Satisfiable', 'Unsatisfiable'}:
        if l_t < r_t:
            wins_left += 1
            left_better.append((r['file'], l_s, l_t, r_s, r_t))
        elif l_t > r_t:
            wins_right += 1
            right_better.append((r['file'], l_s, l_t, r_s, r_t))

print(f'{left_label} vince: {wins_left}')
for item in left_better[:10]:
    file_name, l_s, l_t, r_s, r_t = item
    print(f'  - {file_name}: {left_label}={l_s} ({l_t}) vs {right_label}={r_s} ({r_t})')
print(f'{right_label} vince: {wins_right}')
for item in right_better[:10]:
    file_name, l_s, l_t, r_s, r_t = item
    print(f'  - {file_name}: {left_label}={l_s} ({l_t}) vs {right_label}={r_s} ({r_t})')

print()
print('=== Totale status per colonna ===')
for label, counts in [('left', left_counts), ('right', right_counts)]:
    print(f'[{label}]')
    for key, value in sorted(counts.items()):
        print(f'  {key}: {value}')
PY
