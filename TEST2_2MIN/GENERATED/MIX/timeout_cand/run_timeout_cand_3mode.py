import os
import glob
import time
import csv
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

# Configuration
VAMPIRE = "./build/vampire"
TIMEOUT = 120
TARGET_DIR = "tests/generated/test2mix/fo2_datasets/timeout_cand"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CSV_OUT = os.path.join(SCRIPT_DIR, "timeout_cand_3mode_benchmark_results.csv")
os.makedirs(os.path.dirname(CSV_OUT), exist_ok=True)

# Find all problem files in timeout_cand benchmark
files = sorted(glob.glob(os.path.join(TARGET_DIR, "*.p")))

def classify_output(out_str, is_timeout_expired):
    if is_timeout_expired:
        return "Timeout"
    
    if "% SZS status Satisfiable" in out_str or "% SZS status CounterSatisfiable" in out_str or "Termination reason: Satisfiable" in out_str:
        return "Satisfiable"
    elif "% SZS status Unsatisfiable" in out_str or "% SZS status Theorem" in out_str or "Termination reason: Refutation" in out_str:
        return "Unsatisfiable"
    elif "% SZS status Timeout" in out_str or "Time limit reached" in out_str or "Termination reason: Time limit" in out_str or "Termination reason: TIME_LIMIT" in out_str or "Termination reason: Memory limit" in out_str or "Termination reason: MEMORY_LIMIT" in out_str:
        return "Timeout"
    elif "Vampire Kernel Exception" in out_str or "User error" in out_str or "Aborted" in out_str:
        return "Error"
    else:
        return "Unknown"

def run_single_mode(cmd, timeout):
    t0 = time.time()
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 5)
        elapsed = min(time.time() - t0, float(timeout))
        out = p.stdout + "\n" + p.stderr
        status = classify_output(out, False)
        if status in ["Timeout", "Unknown"] and elapsed >= timeout - 0.5:
            status = "Timeout"
            elapsed = float(timeout)
        return status, elapsed, out
    except subprocess.TimeoutExpired:
        return "Timeout", float(timeout), "TimeoutExpired"

def run_problem(file_path):
    fname = os.path.basename(file_path)
    
    # 1. FO2 Mode
    cmd_fo2 = [VAMPIRE, "-t", str(TIMEOUT), "-m", "4096", "--mode", "fo2", file_path]
    fo2_stat, fo2_time, fo2_out = run_single_mode(cmd_fo2, TIMEOUT)
    
    # 2. Classic Otter Mode
    cmd_cls = [VAMPIRE, "-t", str(TIMEOUT), "-m", "4096", "--saturation_algorithm", "otter", "-av", "off", "-updr", "off", "-bs", "on", "-fsr", "off", file_path]
    cls_stat, cls_time, cls_out = run_single_mode(cmd_cls, TIMEOUT)

    # 3. FO2 Remove Equality Mode
    cmd_req = [VAMPIRE, "-t", str(TIMEOUT), "-m", "4096", "--mode", "fo2_remove_equality", file_path]
    req_stat, req_time, req_out = run_single_mode(cmd_req, TIMEOUT)

    err_out = {}
    if fo2_stat == "Error": err_out["fo2"] = fo2_out
    if cls_stat == "Error": err_out["classic"] = cls_out
    if req_stat == "Error": err_out["remove_eq"] = req_out

    return {
        "FileName": fname,
        "FO2_Status": fo2_stat,
        "FO2_Time": f"{fo2_time:.3f}",
        "Classic_Status": cls_stat,
        "Classic_Time": f"{cls_time:.3f}",
        "FO2_RemoveEq_Status": req_stat,
        "FO2_RemoveEq_Time": f"{req_time:.3f}",
        "Errors": err_out
    }

if __name__ == "__main__":
    print(f"==========================================================================")
    print(f" BENCHMARK COMPARISON ON timeout_cand (3 MODES)")
    print(f" Benchmark Directory : {TARGET_DIR}")
    print(f" Number of Problems  : {len(files)}")
    print(f" Timeout per Problem : {TIMEOUT}s")
    print(f" Parallel Workers    : 8")
    print(f" Modes under test    :")
    print(f"   1. --mode fo2")
    print(f"   2. Classic Otter (--saturation_algorithm otter -av off -updr off -bs on -fsr off)")
    print(f"   3. --mode fo2_remove_equality")
    print(f"==========================================================================\n")
    
    results = []
    errors = []
    
    with ThreadPoolExecutor(max_workers=8) as executor:
        future_to_file = {executor.submit(run_problem, f): f for f in files}
        count = 0
        for future in as_completed(future_to_file):
            count += 1
            res = future.result()
            results.append(res)
            
            if res["Errors"]:
                errors.append((res["FileName"], res["Errors"]))
                
            if count % 50 == 0 or count == len(files):
                print(f"Progresso: {count} / {len(files)} problemi completati...", flush=True)

    # Sort results by filename
    results.sort(key=lambda x: x["FileName"])

    fieldnames = ["FileName", "FO2_Status", "FO2_Time", "Classic_Status", "Classic_Time", "FO2_RemoveEq_Status", "FO2_RemoveEq_Time"]

    with open(CSV_OUT, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in results:
            writer.writerow({k: r[k] for k in fieldnames})

    # Stats calculation
    modes = ["FO2", "Classic", "FO2_RemoveEq"]
    stats = {m: {"SAT": 0, "UNSAT": 0, "TIMEOUT": 0, "UNKNOWN": 0, "ERROR": 0, "TIME_SUM": 0.0} for m in modes}
    
    mismatches = []

    for r in results:
        fn = r["FileName"]
        s_fo2, t_fo2 = r["FO2_Status"], float(r["FO2_Time"])
        s_cls, t_cls = r["Classic_Status"], float(r["Classic_Time"])
        s_req, t_req = r["FO2_RemoveEq_Status"], float(r["FO2_RemoveEq_Time"])

        # Accumulate stats
        for m, status, t_val in zip(modes, [s_fo2, s_cls, s_req], [t_fo2, t_cls, t_req]):
            stats[m]["TIME_SUM"] += t_val
            if status == "Satisfiable": stats[m]["SAT"] += 1
            elif status == "Unsatisfiable": stats[m]["UNSAT"] += 1
            elif status == "Timeout": stats[m]["TIMEOUT"] += 1
            elif status == "Unknown": stats[m]["UNKNOWN"] += 1
            else: stats[m]["ERROR"] += 1

        # Check equisatisfiability mismatches among non-timeout results
        valid_statuses = {s for s in [s_fo2, s_cls, s_req] if s in ["Satisfiable", "Unsatisfiable"]}
        if len(valid_statuses) > 1:
            mismatches.append((fn, s_fo2, t_fo2, s_cls, t_cls, s_req, t_req))

    total = len(results)

    print("\n==========================================================================")
    print(f" SUMMARY RISULTATI BENCHMARK timeout_cand (Timeout={TIMEOUT}s)")
    print(f" Totale Problemi Analizzati: {total}")
    print("--------------------------------------------------------------------------")
    for m in modes:
        st = stats[m]
        avg_t = st["TIME_SUM"] / total if total > 0 else 0.0
        solved = st["SAT"] + st["UNSAT"]
        print(f" {m:<15} : SOLVED={solved:<3} (SAT={st['SAT']:<3} | UNSAT={st['UNSAT']:<3}) | TIMEOUT={st['TIMEOUT']:<3} | UNK={st['UNKNOWN']:<3} | ERR={st['ERROR']:<3} | AvgTime={avg_t:.3f}s")
    
    print("--------------------------------------------------------------------------")
    print(f" Disaccordi di Equisoddisfacibilità (Mismatches): {len(mismatches)}")
    print("==========================================================================")
    
    if len(mismatches) > 0:
        print("\nDettaglio Mismatches:")
        for fn, s1, t1, s2, t2, s3, t3 in mismatches:
            print(f"  {fn}: FO2={s1} ({t1:.2f}s) | Classic={s2} ({t2:.2f}s) | FO2_RemoveEq={s3} ({t3:.2f}s)")

    if len(errors) > 0:
        print(f"\nERRORI RILEVATI: {len(errors)}")
        for fn, err_dict in errors:
            print(f"  {fn}: {err_dict}")

    print(f"\nRisultati salvati in: {CSV_OUT}\n")
