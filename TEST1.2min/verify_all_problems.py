import os
import glob
import subprocess
import re
from concurrent.futures import ThreadPoolExecutor, as_completed

vampire = "./build/vampire"
timeout = 10
files = sorted(glob.glob("tests/generated/test2equality/fo2_datasets/**/*.p", recursive=True))

csv_out = "verification_results.csv"
with open(csv_out, "w") as f:
    f.write("Dataset,FileName,FO2_Status,Classic_Status\n")

def run_problem(file_path):
    fname = os.path.basename(file_path)
    dataset = os.path.basename(os.path.dirname(file_path))
    
    # FO2 Mode
    fo2_cmd = [vampire, "-t", str(timeout), "-m", "4096", "--mode", "fo2", file_path]
    try:
        p1 = subprocess.run(fo2_cmd, capture_output=True, text=True, timeout=timeout+5)
        out1 = p1.stdout + p1.stderr
        if "% SZS status Satisfiable" in out1 or "% SZS status CounterSatisfiable" in out1:
            fo2_stat = "Satisfiable"
        elif "% SZS status Unsatisfiable" in out1 or "% SZS status Theorem" in out1 or re.search(r"Termination reason:\s*Refutation[ \t\r]*$", out1, re.MULTILINE):
            fo2_stat = "Unsatisfiable"
        elif "% SZS status Timeout" in out1 or "Time limit reached" in out1 or "Termination reason: Time limit" in out1:
            fo2_stat = "Timeout"
        else:
            fo2_stat = "Unknown"
    except subprocess.TimeoutExpired:
        fo2_stat = "Timeout"
        
    # Classic Mode
    cls_cmd = [vampire, "-t", str(timeout), "-m", "4096", "--saturation_algorithm", "otter", "-av", "off", "-updr", "off", "-bs", "on", "-fsr", "off", file_path]
    try:
        p2 = subprocess.run(cls_cmd, capture_output=True, text=True, timeout=timeout+5)
        out2 = p2.stdout + p2.stderr
        if "% SZS status Satisfiable" in out2 or "% SZS status CounterSatisfiable" in out2:
            cls_stat = "Satisfiable"
        elif "% SZS status Unsatisfiable" in out2 or "% SZS status Theorem" in out2 or re.search(r"Termination reason:\s*Refutation[ \t\r]*$", out2, re.MULTILINE):
            cls_stat = "Unsatisfiable"
        elif "% SZS status Timeout" in out2 or "Time limit reached" in out2 or "Termination reason: Time limit" in out2:
            cls_stat = "Timeout"
        else:
            cls_stat = "Unknown"
    except subprocess.TimeoutExpired:
        cls_stat = "Timeout"
        
    return dataset, fname, fo2_stat, cls_stat

print(f"Starting verification on {len(files)} files with 16 workers...")
mismatches = []
errors = []
count = 0

with ThreadPoolExecutor(max_workers=16) as executor:
    future_to_file = {executor.submit(run_problem, f): f for f in files}
    for future in as_completed(future_to_file):
        dataset, fname, fo2_stat, cls_stat = future.result()
        count += 1
        with open(csv_out, "a") as f:
            f.write(f"{dataset},{fname},{fo2_stat},{cls_stat}\n")
            
        if fo2_stat in ["Satisfiable", "Unsatisfiable"] and cls_stat in ["Satisfiable", "Unsatisfiable"] and fo2_stat != cls_stat:
            mismatches.append((dataset, fname, fo2_stat, cls_stat))
            print(f"[MISMATCH #{len(mismatches)}] {dataset}/{fname}: FO2={fo2_stat} vs Classic={cls_stat}")
            
        if "Error" in fo2_stat or "Error" in cls_stat:
            errors.append((dataset, fname, fo2_stat, cls_stat))
            print(f"[ERROR #{len(errors)}] {dataset}/{fname}: FO2={fo2_stat} vs Classic={cls_stat}")

print(f"\nFinished testing {count} problems.")
print(f"Total Mismatches: {len(mismatches)}")
print(f"Total Errors: {len(errors)}")
