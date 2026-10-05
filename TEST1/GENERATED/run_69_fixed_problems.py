import os
import glob
import csv
import time
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

vampire = "./build/vampire"
timeout = 30
target_dir = "tests/generated/test2mix/fo2_datasets"
csv_file = "test2mix_benchmark_results.csv"

# Identify the 69 modified problems (in satbuildscaling and unknown_cand)
files_to_run = []

with open(csv_file, "r", encoding="utf-8") as f:
    reader = csv.reader(f)
    header = next(reader)
    for row in reader:
        if not row: continue
        d, fn, s1, t1, s2, t2 = row[0], row[1], row[2], row[3], row[4], row[5]
        if s1 == "Error" or s2 == "Error":
            fpath = os.path.join(target_dir, d, fn)
            if os.path.exists(fpath):
                files_to_run.append((d, fn, fpath))

print(f"Found {len(files_to_run)} problem files to re-evaluate with 30s timeout...")

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

def run_problem(d, fn, file_path):
    # FO2 Mode
    fo2_cmd = [vampire, "-t", str(timeout), "-m", "4096", "--mode", "fo2", file_path]
    t0 = time.time()
    try:
        p1 = subprocess.run(fo2_cmd, capture_output=True, text=True, timeout=timeout+5)
        fo2_time = min(time.time() - t0, float(timeout))
        out1 = p1.stdout + "\n" + p1.stderr
        fo2_stat = classify_output(out1, p1.returncode, False)
        if fo2_stat == "Timeout":
            fo2_time = float(timeout)
    except subprocess.TimeoutExpired:
        fo2_stat = "Timeout"
        fo2_time = float(timeout)
        
    # Classic Mode
    cls_cmd = [vampire, "-t", str(timeout), "-m", "4096", "--saturation_algorithm", "otter", "-av", "off", "-updr", "off", "-bs", "on", "-fsr", "off", file_path]
    t0 = time.time()
    try:
        p2 = subprocess.run(cls_cmd, capture_output=True, text=True, timeout=timeout+5)
        cls_time = min(time.time() - t0, float(timeout))
        out2 = p2.stdout + "\n" + p2.stderr
        cls_stat = classify_output(out2, p2.returncode, False)
        if cls_stat == "Timeout":
            cls_time = float(timeout)
    except subprocess.TimeoutExpired:
        cls_stat = "Timeout"
        cls_time = float(timeout)
        
    return d, fn, fo2_stat, f"{fo2_time:.3f}", cls_stat, f"{cls_time:.3f}"

new_results = {}
with ThreadPoolExecutor(max_workers=8) as executor:
    future_map = {executor.submit(run_problem, d, fn, fp): (d, fn) for d, fn, fp in files_to_run}
    count = 0
    for future in as_completed(future_map):
        count += 1
        d, fn, s1, t1, s2, t2 = future.result()
        new_results[(d, fn)] = (d, fn, s1, t1, s2, t2)
        print(f"[{count}/{len(files_to_run)}] {d}/{fn}: FO2={s1} ({t1}s) vs Classic={s2} ({t2}s)")

# Now update test2mix_benchmark_results.csv with the new results
all_rows = []
with open(csv_file, "r", encoding="utf-8") as f:
    reader = csv.reader(f)
    header = next(reader)
    for row in reader:
        if not row: continue
        d, fn = row[0], row[1]
        if (d, fn) in new_results:
            all_rows.append(new_results[(d, fn)])
        else:
            all_rows.append(row)

# Sort rows
all_rows.sort(key=lambda x: (x[0], x[1]))

with open(csv_file, "w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerow(header)
    writer.writerows(all_rows)

print("\nSuccessfully updated test2mix_benchmark_results.csv with new results!")
