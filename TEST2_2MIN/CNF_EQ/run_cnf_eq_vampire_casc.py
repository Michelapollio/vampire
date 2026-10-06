#!/usr/bin/env python3
import os
import glob
import subprocess
import time
import sys

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
VAMPIRE_DIR = os.path.abspath(os.path.join(BASE_DIR, "..", ".."))
VAMPIRE_BIN = os.path.join(VAMPIRE_DIR, "build", "vampire")
LIST_FILE = os.path.join(BASE_DIR, "CNF_EQ_final_problems.txt")
CSV_OUTPUT = os.path.join(BASE_DIR, "CNF_EQ_vampire_casc_results.csv")
TPTP_DIR = "/home/michela/Scaricati/TPTP-v9.2.1"

SEARCH_DIRS = [
    os.path.join(VAMPIRE_DIR, "tptp_classified", "fo2_with_equality"),
    os.path.join(VAMPIRE_DIR, "tests", "tptp_raw", "Problems"),
    os.path.join(VAMPIRE_DIR, "tests", "generated"),
    os.path.join(TPTP_DIR, "Problems"),
]

def find_problem(filename):
    for sdir in SEARCH_DIRS:
        matches = glob.glob(os.path.join(sdir, "**", filename), recursive=True)
        if matches:
            return matches[0]
    return None

def classify_status(stdout_text):
    if "Termination reason: Refutation" in stdout_text or "% Refutation found" in stdout_text or "SZS status Unsatisfiable" in stdout_text or "SZS status Theorem" in stdout_text:
        return "Unsatisfiable"
    elif "Termination reason: Satisfiable" in stdout_text or "% Satisfiable" in stdout_text or "SZS status Satisfiable" in stdout_text or "SZS status CounterSatisfiable" in stdout_text:
        return "Satisfiable"
    elif "Termination reason: Time limit" in stdout_text or "Time limit reached" in stdout_text or "SZS status Timeout" in stdout_text:
        return "Timeout"
    elif "Termination reason: Memory limit" in stdout_text or "Memory limit reached" in stdout_text:
        return "MemoryOut"
    else:
        for line in stdout_text.splitlines():
            if "Termination reason:" in line:
                return line.split("Termination reason:")[-1].strip()
            elif "% SZS status" in line:
                return line.split("% SZS status")[-1].strip()
        return "Unknown"

def main():
    timeout = sys.argv[1] if len(sys.argv) > 1 else "120"
    
    if not os.path.exists(LIST_FILE):
        print(f"Error: {LIST_FILE} not found!")
        sys.exit(1)

    with open(LIST_FILE, 'r') as f:
        problems = [line.strip() for line in f if line.strip() and not line.startswith('%')]

    print(f"============================================================")
    print(f" Benchmark CNF_EQ in Modalità Vampire Normale (CASC Portfolio)")
    print(f" Totale Problemi: {len(problems)} | Timeout: {timeout}s")
    print(f" Output CSV: {CSV_OUTPUT}")
    print(f"============================================================")

    # Open CSV output file
    with open(CSV_OUTPUT, 'w') as csv_out:
        csv_out.write("FileName,Vampire_CASC_Status,Vampire_CASC_Time\n")
        csv_out.flush()

        for idx, prob_name in enumerate(problems, 1):
            prob_path = find_problem(prob_name)
            if not prob_path:
                print(f"[{idx}/{len(problems)}] {prob_name}: NOT FOUND")
                csv_out.write(f"{prob_name},NotFound,0.000\n")
                csv_out.flush()
                continue

            cmd = [
                VAMPIRE_BIN,
                "-t", timeout,
                "-m", "4096",
                "--include", TPTP_DIR,
                "--mode", "casc",
                prob_path
            ]

            print(f"[{idx}/{len(problems)}] Running {prob_name}...", end="", flush=True)
            t_start = time.perf_counter()
            try:
                proc = subprocess.run(cmd, capture_output=True, text=True)
                t_end = time.perf_counter()
                elapsed = t_end - t_start
                status = classify_status(proc.stdout)
            except Exception as e:
                t_end = time.perf_counter()
                elapsed = t_end - t_start
                status = f"ExecutionError: {e}"

            print(f" Result: {status} ({elapsed:.3f}s)")
            csv_out.write(f"{prob_name},{status},{elapsed:.3f}\n")
            csv_out.flush()

    print(f"\nBenchmark completato! Risultati salvati in: {CSV_OUTPUT}")

if __name__ == "__main__":
    main()
