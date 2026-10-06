import os
import glob
import time
import subprocess
import re
from concurrent.futures import ThreadPoolExecutor, as_completed

vampire = "./build/vampire"
timeout = 30
files = sorted(glob.glob("tests/generated/test2equality/fo2_datasets/**/*.p", recursive=True))

csv_out = "test2equality_benchmark_results.csv"

def run_problem(file_path):
    fname = os.path.basename(file_path)
    dataset = os.path.basename(os.path.dirname(file_path))
    
    # FO2 Mode
    fo2_cmd = [vampire, "-t", str(timeout), "-m", "4096", "--mode", "fo2", file_path]
    t0 = time.time()
    try:
        p1 = subprocess.run(fo2_cmd, capture_output=True, text=True, timeout=timeout+5)
        fo2_time = min(time.time() - t0, float(timeout))
        out1 = p1.stdout + p1.stderr
        if "% SZS status Satisfiable" in out1 or "% SZS status CounterSatisfiable" in out1 or "Termination reason: Satisfiable" in out1:
            fo2_stat = "Satisfiable"
        elif "% SZS status Unsatisfiable" in out1 or "% SZS status Theorem" in out1 or re.search(r"Termination reason:\s*Refutation[ \t\r]*$", out1, re.MULTILINE):
            fo2_stat = "Unsatisfiable"
        elif "% SZS status Timeout" in out1 or "Time limit reached" in out1 or "Termination reason: Time limit" in out1:
            fo2_stat = "Timeout"
            fo2_time = float(timeout)
        else:
            fo2_stat = "Unknown"
    except subprocess.TimeoutExpired:
        fo2_stat = "Timeout"
        fo2_time = float(timeout)
        
    # Classic Mode
    cls_cmd = [vampire, "-t", str(timeout), "-m", "4096", "--saturation_algorithm", "otter", "-av", "off", "-updr", "off", "-bs", "on", "-fsr", "off", file_path]
    t0 = time.time()
    try:
        p2 = subprocess.run(cls_cmd, capture_output=True, text=True, timeout=timeout+5)
        cls_time = min(time.time() - t0, float(timeout))
        out2 = p2.stdout + p2.stderr
        if "% SZS status Satisfiable" in out2 or "% SZS status CounterSatisfiable" in out2 or "Termination reason: Satisfiable" in out2:
            cls_stat = "Satisfiable"
        elif "% SZS status Unsatisfiable" in out2 or "% SZS status Theorem" in out2 or re.search(r"Termination reason:\s*Refutation[ \t\r]*$", out2, re.MULTILINE):
            cls_stat = "Unsatisfiable"
        elif "% SZS status Timeout" in out2 or "Time limit reached" in out2 or "Termination reason: Time limit" in out2:
            cls_stat = "Timeout"
            cls_time = float(timeout)
        else:
            cls_stat = "Unknown"
    except subprocess.TimeoutExpired:
        cls_stat = "Timeout"
        cls_time = float(timeout)
        
    return dataset, fname, fo2_stat, f"{fo2_time:.3f}", cls_stat, f"{cls_time:.3f}"

print(f"Starting parallel benchmark on {len(files)} files with TIMEOUT={timeout}s (16 workers)...")

results = []
with ThreadPoolExecutor(max_workers=16) as executor:
    future_to_file = {executor.submit(run_problem, f): f for f in files}
    count = 0
    for future in as_completed(future_to_file):
        count += 1
        res = future.result()
        results.append(res)
        if count % 20 == 0 or count == len(files):
            print(f"Processed {count} / {len(files)} problems...")

# Sort results by dataset and filename to match bash script output
results.sort(key=lambda x: (x[0], x[1]))

with open(csv_out, "w") as f:
    f.write("Dataset,FileName,FO2_Status,FO2_Time,Classic_Status,Classic_Time\n")
    for d, fn, s1, t1, s2, t2 in results:
        f.write(f"{d},{fn},{s1},{t1},{s2},{t2}\n")

# Summary stats
fo2_sat = fo2_unsat = fo2_to = fo2_err = 0
cls_sat = cls_unsat = cls_to = cls_err = 0
fo2_time_sum = 0.0
cls_time_sum = 0.0
mismatches = []

for d, fn, s1, t1, s2, t2 in results:
    t1_f = float(t1)
    t2_f = float(t2)
    fo2_time_sum += t1_f
    cls_time_sum += t2_f
    
    if s1 == "Satisfiable": fo2_sat += 1
    elif s1 == "Unsatisfiable": fo2_unsat += 1
    elif s1 == "Timeout": fo2_to += 1
    else: fo2_err += 1
    
    if s2 == "Satisfiable": cls_sat += 1
    elif s2 == "Unsatisfiable": cls_unsat += 1
    elif s2 == "Timeout": cls_to += 1
    else: cls_err += 1
    
    if s1 in ["Satisfiable", "Unsatisfiable"] and s2 in ["Satisfiable", "Unsatisfiable"] and s1 != s2:
        mismatches.append((d, fn, s1, t1, s2, t2))

total = len(results)
fo2_avg = fo2_time_sum / total if total > 0 else 0
cls_avg = cls_time_sum / total if total > 0 else 0

print("\n==========================================================================")
print(" BENCHMARK SUMMARY FOR tests/generated/test2equality (Timeout=30s)")
print(f" Total Problems: {total}")
print("--------------------------------------------------------------------------")
print(f" FO2 Mode     : SAT={fo2_sat} | UNSAT={fo2_unsat} | TIMEOUT={fo2_to} | ERR={fo2_err} | AvgTime={fo2_avg:.3f}s")
print(f" Classic Mode : SAT={cls_sat} | UNSAT={cls_unsat} | TIMEOUT={cls_to} | ERR={cls_err} | AvgTime={cls_avg:.3f}s")
print(f" Equisatisfiability Mismatches: {len(mismatches)}")
print("==========================================================================")
if len(mismatches) > 0:
    print("\nMismatches:")
    for d, fn, s1, t1, s2, t2 in mismatches:
        print(f"  {d}/{fn}: FO2={s1} ({t1}s) vs Classic={s2} ({t2}s)")
print(f"Results saved to {csv_out}")
