import sys
import os
import subprocess

vampire = "./build/vampire"

if len(sys.argv) < 2:
    print("Uso: python3 diagnose_non_equisat.py <percorso_problema.p>")
    sys.exit(1)

problem_path = sys.argv[1]

if not os.path.exists(problem_path):
    print(f"Errore: file non trovato -> {problem_path}")
    sys.exit(1)

cmd = [vampire, "-t", "30", "-m", "4096", "--mode", "fo2_step_by_step", problem_path]
res = subprocess.run(cmd, capture_output=True, text=True)

print(res.stdout)
if res.stderr:
    print("--- STDERR ---")
    print(res.stderr)
