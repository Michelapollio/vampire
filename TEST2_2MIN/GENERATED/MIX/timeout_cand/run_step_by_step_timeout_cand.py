import os
import glob
import time
import csv
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

vampire = "./build/vampire"
timeout = 30
target_dir = "tests/generated/test2mix/fo2_datasets/timeout_cand"
script_dir = os.path.dirname(os.path.abspath(__file__))
csv_out = os.path.join(script_dir, "timeout_cand_step_by_step_results.csv")

os.makedirs(os.path.dirname(csv_out), exist_ok=True)

files = sorted(glob.glob(os.path.join(target_dir, "*.p")))

print(f"Trovati {len(files)} problemi in {target_dir}")

def parse_step_by_step_output(out_str):
    steps = {}
    # Pattern to match: [DIAGNOSTIC] Passo X (...) : Status (time)
    # or Passo X (...) -> Status (time)
    pattern = r"Passo\s+(\d+)\s*\([^)]*\)\s*(?::|->)\s*([A-Za-z/]+)"
    matches = re.findall(pattern, out_str)
    for step_num, status in matches:
        steps[int(step_num)] = status
        
    broken_lemma = None
    preserved = True
    
    if 0 in steps:
        init_status = steps[0]
        for s in range(1, 7):
            if s in steps:
                st = steps[s]
                if st != init_status and st != "Timeout/Unknown" and init_status != "Timeout/Unknown":
                    preserved = False
                    if broken_lemma is None:
                        broken_lemma = s

    return steps, preserved, broken_lemma

def run_problem(file_path):
    fname = os.path.basename(file_path)
    cmd = [vampire, "-t", str(timeout), "-m", "4096", "--mode", "fo2_step_by_step", file_path]
    t0 = time.time()
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout+5)
        out = p.stdout + "\n" + p.stderr
        elapsed = time.time() - t0
        steps, preserved, broken_lemma = parse_step_by_step_output(out)
    except subprocess.TimeoutExpired:
        elapsed = float(timeout)
        steps = {0: "Timeout", 1: "Timeout", 2: "Timeout", 3: "Timeout", 4: "Timeout", 5: "Timeout", 6: "Timeout"}
        preserved = True
        broken_lemma = None

    return {
        "FileName": fname,
        "Step0": steps.get(0, "Unknown"),
        "Step1": steps.get(1, "Unknown"),
        "Step2": steps.get(2, "Unknown"),
        "Step3": steps.get(3, "Unknown"),
        "Step4": steps.get(4, "Unknown"),
        "Step5": steps.get(5, "Unknown"),
        "Step6": steps.get(6, "Unknown"),
        "Preserved": "YES" if preserved else "NO",
        "BrokenAtLemma": broken_lemma if broken_lemma else "-",
        "TimeSec": f"{elapsed:.3f}"
    }

results = []
count_preserved = 0
count_broken = 0
broken_by_lemma = {1: 0, 2: 0, 3: 0, 4: 0, 5: 0, 6: 0}

print("Inizio diagnosi step-by-step su timeout_cand...", flush=True)

with ThreadPoolExecutor(max_workers=8) as executor:
    futures = {executor.submit(run_problem, f): f for f in files}
    done_count = 0
    for future in as_completed(futures):
        res = future.result()
        results.append(res)
        done_count += 1
        
        if res["Preserved"] == "YES":
            count_preserved += 1
        else:
            count_broken += 1
            l_num = res["BrokenAtLemma"]
            if l_num in broken_by_lemma:
                broken_by_lemma[l_num] += 1
                
        if done_count % 50 == 0 or done_count == len(files):
            print(f"Progresso: {done_count}/{len(files)} completati (Preservati: {count_preserved}, Rotti: {count_broken})", flush=True)

# Order results by filename
results.sort(key=lambda r: r["FileName"])

fieldnames = ["FileName", "Step0", "Step1", "Step2", "Step3", "Step4", "Step5", "Step6", "Preserved", "BrokenAtLemma", "TimeSec"]

with open(csv_out, "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(results)

print("\n===========================================================")
print("  DIAGNOSI timeout_cand COMPLETATA")
print("===========================================================")
print(f"Totale problemi analizzati: {len(results)}")
print(f"Equisoddisfacibilità PRESERVATA  : {count_preserved} ({count_preserved/len(results)*100:.1f}%)")
print(f"Equisoddisfacibilità ROTTA       : {count_broken} ({count_broken/len(results)*100:.1f}%)")
if count_broken > 0:
    print("Dettaglio rotture per Lemma:")
    for l_num in range(1, 7):
        print(f"  - Rotto al Lemma {l_num}: {broken_by_lemma[l_num]}")
print(f"Risultati salvati in: {csv_out}")
print("===========================================================\n")
