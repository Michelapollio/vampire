import os
import glob
import time
import csv
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

vampire = "./build/vampire"
timeout = 30
target_dir = "tests/generated/test2mix/fo2_datasets"
script_dir = os.path.dirname(os.path.abspath(__file__))
csv_path = os.path.join(script_dir, "test2mix_benchmark_results.csv")

def classify_output(out_str, returncode, is_timeout_expired):
    if is_timeout_expired:
        return "Timeout"
    
    if "% SZS status Satisfiable" in out_str or "% SZS status CounterSatisfiable" in out_str or "Termination reason: Satisfiable" in out_str:
        return "Satisfiable"
    elif "% SZS status Unsatisfiable" in out_str or "% SZS status Theorem" in out_str or "Termination reason: Refutation" in out_str:
        return "Unsatisfiable"
    elif "% SZS status Timeout" in out_str or "Time limit reached" in out_str or "Termination reason: Time limit" in out_str or "Termination reason: TIME_LIMIT" in out_str or "Termination reason: Memory limit" in out_str or "Termination reason: MEMORY_LIMIT" in out_str:
        return "Timeout"
    else:
        return "Unknown"

# Build file map: (dataset, filename) -> file_path
file_map = {}
for p in glob.glob(os.path.join(target_dir, "**/*.p"), recursive=True):
    fname = os.path.basename(p)
    ds = os.path.basename(os.path.dirname(p))
    file_map[(ds, fname)] = p

with open(csv_path, "r", newline="") as f:
    reader = csv.DictReader(f)
    rows = list(reader)
    fieldnames = reader.fieldnames

mismatches = []
for i, r in enumerate(rows):
    if r["FO2_Status"] != r["Classic_Status"]:
        mismatches.append((i, r))

print(f"Total rows in CSV: {len(rows)}")
print(f"Total mismatches to re-run: {len(mismatches)}")

def run_fo2(item):
    idx, r = item
    ds = r["Dataset"]
    fname = r["FileName"]
    fpath = file_map.get((ds, fname))
    if not fpath:
        # Fallback search
        matches = glob.glob(os.path.join(target_dir, "**", fname), recursive=True)
        if matches:
            fpath = matches[0]
        else:
            print(f"File not found for {ds}/{fname}")
            return idx, r["FO2_Status"], r["FO2_Time"]

    fo2_cmd = [vampire, "-t", str(timeout), "-m", "4096", "--mode", "fo2", fpath]
    t0 = time.time()
    try:
        p = subprocess.run(fo2_cmd, capture_output=True, text=True, timeout=timeout+5)
        fo2_time = min(time.time() - t0, float(timeout))
        out = p.stdout + "\n" + p.stderr
        fo2_stat = classify_output(out, p.returncode, False)
        if fo2_stat == "Timeout":
            fo2_time = float(timeout)
    except subprocess.TimeoutExpired:
        fo2_stat = "Timeout"
        fo2_time = float(timeout)
        
    return idx, fo2_stat, f"{fo2_time:.3f}"

updated_count = 0
now_matching = 0
still_mismatch = 0

with ThreadPoolExecutor(max_workers=8) as executor:
    futures = [executor.submit(run_fo2, item) for item in mismatches]
    for future in as_completed(futures):
        idx, new_stat, new_time = future.result()
        old_fo2 = rows[idx]["FO2_Status"]
        cls = rows[idx]["Classic_Status"]
        rows[idx]["FO2_Status"] = new_stat
        rows[idx]["FO2_Time"] = new_time
        updated_count += 1
        
        if new_stat == cls:
            now_matching += 1
        else:
            still_mismatch += 1

with open(csv_path, "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(rows)

print("\n--- RECHECK COMPLETED ---")
print(f"Updated {updated_count} rows in {csv_path}")
print(f"Newly Matching with Classic Mode: {now_matching}")
print(f"Still Mismatching / Timeout: {still_mismatch}")
