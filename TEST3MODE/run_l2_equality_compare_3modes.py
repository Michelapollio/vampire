#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import tempfile

VAMPIRE = "./build/vampire"
TPTP = "/home/michela/Scaricati/TPTP-v9.2.1"
LIST_FILE = "TEST1/L2_equality_final.txt"
if not os.path.exists(LIST_FILE):
    if os.path.exists("TEST2/L2_equality_final.txt"):
        LIST_FILE = "TEST2/L2_equality_final.txt"
    elif os.path.exists("L2_equality_final.txt"):
        LIST_FILE = "L2_equality_final.txt"

OUTPUT_CSV = "TEST2/l2_equality_3modes_results.csv"
TIMEOUT = 120

import re

def classify_output(out_str, is_timeout):
    if is_timeout:
        return "Timeout"
    if "% SZS status Satisfiable" in out_str or "% SZS status CounterSatisfiable" in out_str or "Termination reason: Satisfiable" in out_str:
        return "Satisfiable"
    elif "% SZS status Unsatisfiable" in out_str or "% SZS status Theorem" in out_str or "% SZS status ContradictoryAxioms" in out_str or re.search(r"Termination reason:\s*Refutation[ \t\r]*$", out_str, re.MULTILINE):
        return "Unsatisfiable"
    elif "% SZS status Timeout" in out_str or "Time limit reached" in out_str or "Termination reason: Time limit" in out_str or "Termination reason: TIME_LIMIT" in out_str or "Termination reason: Memory limit" in out_str or "Termination reason: MEMORY_LIMIT" in out_str:
        return "Timeout"
    else:
        return "Unknown"

def main():
    if len(sys.argv) > 1:
        list_file = sys.argv[1]
    else:
        list_file = LIST_FILE

    if len(sys.argv) > 2:
        timeout = int(sys.argv[2])
    else:
        timeout = TIMEOUT

    if not os.path.exists(VAMPIRE):
        print(f"Errore: eseguibile Vampire non trovato in {VAMPIRE}")
        sys.exit(1)

    if not os.path.exists(list_file):
        print(f"Errore: file lista {list_file} non trovato")
        sys.exit(1)

    with open(list_file, "r") as f:
        problems = [line.strip() for line in f if line.strip() and not line.startswith("%")]

    print("==========================================================================")
    print(f" BENCHMARK CONFRONTO 3 MODALITÀ (FO2 con Uguaglianza)")
    print(f" Lista Problemi: {list_file} ({len(problems)} problemi)")
    print(f" Timeout per modalità: {timeout}s")
    print(" 1. Mode FO2 (--mode fo2)")
    print(" 2. Mode Classic Otter (--saturation_algorithm otter -av off -updr off -bs on -fsr off)")
    print(" 3. Mode FO2_RemoveEquality + Otter (--mode fo2_remove_equality --saturation_algorithm otter -av off -updr off -bs on -fsr off)")
    print("==========================================================================")

    results = []

    for idx, prob_name in enumerate(problems, 1):
        domain = prob_name[:3]
        problem_path = os.path.join(TPTP, "Problems", domain, prob_name)

        if not os.path.exists(problem_path):
            print(f"[{idx}/{len(problems)}] [WARNING] File non trovato: {problem_path}")
            continue

        print(f"\n[{idx}/{len(problems)}] Esecuzione su: {prob_name}...")

        # -------------------------------------------------------------
        # 1. Mode FO2
        # -------------------------------------------------------------
        cmd_fo2 = [VAMPIRE, "-t", str(timeout), "-m", "4096", "--include", TPTP, "--mode", "fo2", problem_path]
        t0 = time.time()
        try:
            p1 = subprocess.run(cmd_fo2, capture_output=True, text=True, timeout=timeout+5)
            fo2_time = min(time.time() - t0, float(timeout))
            out1 = p1.stdout + "\n" + p1.stderr
            fo2_stat = classify_output(out1, False)
            if fo2_stat == "Timeout":
                fo2_time = float(timeout)
        except subprocess.TimeoutExpired:
            fo2_stat = "Timeout"
            fo2_time = float(timeout)
        
        print(f"  -> 1. Mode FO2          : {fo2_stat} ({fo2_time:.3f}s)")

        # -------------------------------------------------------------
        # 2. Mode Classic Otter
        # -------------------------------------------------------------
        cmd_cls = [VAMPIRE, "-t", str(timeout), "-m", "4096", "--include", TPTP, "--saturation_algorithm", "otter", "-av", "off", "-updr", "off", "-bs", "on", "-fsr", "off", problem_path]
        t0 = time.time()
        try:
            p2 = subprocess.run(cmd_cls, capture_output=True, text=True, timeout=timeout+5)
            cls_time = min(time.time() - t0, float(timeout))
            out2 = p2.stdout + "\n" + p2.stderr
            cls_stat = classify_output(out2, False)
            if cls_stat == "Timeout":
                cls_time = float(timeout)
        except subprocess.TimeoutExpired:
            cls_stat = "Timeout"
            cls_time = float(timeout)

        print(f"  -> 2. Mode Classic Otter: {cls_stat} ({cls_time:.3f}s)")

        # -------------------------------------------------------------
        # 3. Mode FO2_RemoveEquality + Vampire
        # -------------------------------------------------------------
        cmd_req = [VAMPIRE, "-t", str(timeout), "-m", "4096", "--include", TPTP, "--mode", "fo2_remove_equality", problem_path]
        t0 = time.time()
        try:
            p3 = subprocess.run(cmd_req, capture_output=True, text=True, timeout=timeout+5)
            req_vamp_time = min(time.time() - t0, float(timeout))
            out3 = p3.stdout + "\n" + p3.stderr
            req_vamp_stat = classify_output(out3, False)
            if req_vamp_stat == "Timeout":
                req_vamp_time = float(timeout)
        except subprocess.TimeoutExpired:
            req_vamp_stat = "Timeout"
            req_vamp_time = float(timeout)

        print(f"  -> 3. Mode FO2_RemoveEq + Vampire: {req_vamp_stat} ({req_vamp_time:.3f}s)")

        results.append({
            "FileName": prob_name,
            "FO2_Status": fo2_stat,
            "FO2_Time": f"{fo2_time:.3f}",
            "Classic_Status": cls_stat,
            "Classic_Time": f"{cls_time:.3f}",
            "FO2_RemoveEq_Vampire_Status": req_vamp_stat,
            "FO2_RemoveEq_Vampire_Time": f"{req_vamp_time:.3f}"
        })

    # Write CSV
    os.makedirs(os.path.dirname(OUTPUT_CSV), exist_ok=True)
    with open(OUTPUT_CSV, "w") as f:
        f.write("FileName,FO2_Status,FO2_Time,Classic_Status,Classic_Time,FO2_RemoveEq_Vampire_Status,FO2_RemoveEq_Vampire_Time\n")
        for r in results:
            f.write(f"{r['FileName']},{r['FO2_Status']},{r['FO2_Time']},{r['Classic_Status']},{r['Classic_Time']},{r['FO2_RemoveEq_Vampire_Status']},{r['FO2_RemoveEq_Vampire_Time']}\n")

    print("\n==========================================================================")
    print(" BENCHMARK RECAP")
    print(f" Total Problems Analyzed: {len(results)}")
    print(f" CSV salvato in: {OUTPUT_CSV}")
    print("==========================================================================")

if __name__ == "__main__":
    main()
