import os
import glob
import time
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

vampire = "./build/vampire"
timeout = 30
target_dir = "tests/generated/test2mix/fo2_datasets"
csv_out = "test2mix_benchmark_results.csv"

files = sorted(glob.glob(os.path.join(target_dir, "**/*.p"), recursive=True))

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

def run_problem(file_path):
    fname = os.path.basename(file_path)
    dataset = os.path.basename(os.path.dirname(file_path))
    
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
        out1 = "TimeoutExpired"
        
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
        out2 = "TimeoutExpired"
        
    return dataset, fname, fo2_stat, f"{fo2_time:.3f}", cls_stat, f"{cls_time:.3f}", file_path, (out1 if fo2_stat == "Error" else None), (out2 if cls_stat == "Error" else None)

if __name__ == "__main__":
    print(f"Starting parallel benchmark on {len(files)} test2mix files with TIMEOUT={timeout}s (8 workers)...")
    
    results = []
    errors = []
    
    with ThreadPoolExecutor(max_workers=8) as executor:
        future_to_file = {executor.submit(run_problem, f): f for f in files}
        count = 0
        for future in as_completed(future_to_file):
            count += 1
            res = future.result()
            dataset, fname, fo2_stat, fo2_time, cls_stat, cls_time, file_path, fo2_err_out, cls_err_out = res
            results.append((dataset, fname, fo2_stat, fo2_time, cls_stat, cls_time))
            
            if fo2_stat == "Error" or cls_stat == "Error":
                errors.append((dataset, fname, fo2_stat, cls_stat, fo2_err_out, cls_err_out))
                
            if count % 50 == 0 or count == len(files):
                print(f"Processed {count} / {len(files)} problems...")

    # Sort results by dataset and filename
    results.sort(key=lambda x: (x[0], x[1]))

    with open(csv_out, "w") as f:
        f.write("Dataset,FileName,FO2_Status,FO2_Time,Classic_Status,Classic_Time\n")
        for d, fn, s1, t1, s2, t2 in results:
            f.write(f"{d},{fn},{s1},{t1},{s2},{t2}\n")

    # Summary stats
    fo2_sat = fo2_unsat = fo2_to = fo2_unk = fo2_err = 0
    cls_sat = cls_unsat = cls_to = cls_unk = cls_err = 0
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
        elif s1 == "Unknown": fo2_unk += 1
        else: fo2_err += 1
        
        if s2 == "Satisfiable": cls_sat += 1
        elif s2 == "Unsatisfiable": cls_unsat += 1
        elif s2 == "Timeout": cls_to += 1
        elif s2 == "Unknown": cls_unk += 1
        else: cls_err += 1
        
        if s1 in ["Satisfiable", "Unsatisfiable"] and s2 in ["Satisfiable", "Unsatisfiable"] and s1 != s2:
            mismatches.append((d, fn, s1, t1, s2, t2))

    total = len(results)
    fo2_avg = fo2_time_sum / total if total > 0 else 0
    cls_avg = cls_time_sum / total if total > 0 else 0

    print("\n==========================================================================")
    print(f" BENCHMARK SUMMARY FOR test2mix (Timeout={timeout}s)")
    print(f" Total Problems: {total}")
    print("--------------------------------------------------------------------------")
    print(f" FO2 Mode     : SAT={fo2_sat} | UNSAT={fo2_unsat} | TIMEOUT={fo2_to} | UNKNOWN={fo2_unk} | ERR={fo2_err} | AvgTime={fo2_avg:.3f}s")
    print(f" Classic Mode : SAT={cls_sat} | UNSAT={cls_unsat} | TIMEOUT={cls_to} | UNKNOWN={cls_unk} | ERR={cls_err} | AvgTime={cls_avg:.3f}s")
    print(f" Equisatisfiability Mismatches: {len(mismatches)}")
    print("==========================================================================")
    if len(mismatches) > 0:
        print("\nMismatches:")
        for d, fn, s1, t1, s2, t2 in mismatches:
            print(f"  {d}/{fn}: FO2={s1} ({t1}s) vs Classic={s2} ({t2}s)")
    if len(errors) > 0:
        print(f"\nERRORS DETECTED: {len(errors)}")
        for d, fn, s1, s2, e1, e2 in errors:
            print(f"  {d}/{fn}: FO2={s1}, Classic={s2}")
            if e1: print(f"    FO2 Output: {e1[:200]}...")
            if e2: print(f"    Classic Output: {e2[:200]}...")
    print(f"Results saved to {csv_out}")
