#!/bin/bash

# Script per l'esecuzione del Benchmark Comparativo (Fase 2)
# Confronta Classic Vampire vs FO2 Vampire sui file di un dataset.
# Usage: ./scripts/run_comparative_benchmark.sh [DATASET_DIR] [TIME_LIMIT_SEC] [OUTPUT_PREFIX] [JOBS]

DATASET_DIR="${1:-tests/fo2_datasets/tptp/fof_equality}"
TIME_LIMIT="${2:-1800}" # Default: 1800s (30 minuti)
OUTPUT_PREFIX="${3:-confronto_fof_eq}"
JOBS="${4:-$(nproc 2>/dev/null || echo 4)}"
VAMPIRE_EXEC="./build/vampire"

if [ ! -x "$VAMPIRE_EXEC" ]; then
    echo "Errore: Eseguibile Vampire non trovato in $VAMPIRE_EXEC"
    exit 1
fi

if [ ! -d "$DATASET_DIR" ]; then
    echo "Errore: Directory $DATASET_DIR non trovata."
    exit 1
fi

LOG_FILE="${OUTPUT_PREFIX}.log"
CSV_FILE="${OUTPUT_PREFIX}.csv"

rm -f "$LOG_FILE" "$CSV_FILE"

FAIR_FLAGS="${FAIR_FLAGS:---sa otter -av off -updr off -fs off}"

MEM_LIMIT="${6:-4096}" # Default: 4096 MB (4GB) per processo

echo "=================================================="
echo "Benchmark Comparativo FO2 (Fase 2 - Fair Comparison)"
echo "Dataset: $DATASET_DIR"
echo "Time Limit: ${TIME_LIMIT}s"
echo "Memory Limit: ${MEM_LIMIT}MB"
echo "Fair Flags Classic: $FAIR_FLAGS"
echo "Parallel Jobs: $JOBS"
echo "Log: $LOG_FILE"
echo "CSV: $CSV_FILE"
echo "=================================================="

echo "File,Classic_Status,Classic_Time,FO2_Status,FO2_Time" > "$CSV_FILE"

TPTP_DIR="${5:-tests/tptp_raw}"
export VAMPIRE_EXEC TIME_LIMIT MEM_LIMIT TPTP_DIR FAIR_FLAGS

run_single_test() {
    fpath="$1"
    fname=$(basename "$fpath")

    # Run Classic Vampire (Fair Baseline: --sa otter -av off -updr off -fs off -m 4096)
    CLASSIC_OUT=$("$VAMPIRE_EXEC" --include "$TPTP_DIR" -m "$MEM_LIMIT" $FAIR_FLAGS --time_limit "$TIME_LIMIT" "$fpath" 2>&1)
    CLASSIC_STATUS=$(echo "$CLASSIC_OUT" | grep "% Termination reason:" | head -n 1 | awk -F': ' '{print $2}')
    CLASSIC_TIME=$(echo "$CLASSIC_OUT" | grep "% Time elapsed:" | head -n 1 | awk '{print $4}')
    if [ -z "$CLASSIC_STATUS" ]; then
        if echo "$CLASSIC_OUT" | grep -qE "cannot open file|Parse error|Fatal error"; then
            CLASSIC_STATUS="Error"
            CLASSIC_TIME="0"
        else
            CLASSIC_STATUS="Timeout"
            CLASSIC_TIME="$TIME_LIMIT"
        fi
    fi

    # Run FO2 Vampire (-m MEM_LIMIT)
    FO2_OUT=$("$VAMPIRE_EXEC" --include "$TPTP_DIR" -m "$MEM_LIMIT" --mode fo2 --time_limit "$TIME_LIMIT" "$fpath" 2>&1)
    FO2_STATUS=$(echo "$FO2_OUT" | grep "% Termination reason:" | head -n 1 | awk -F': ' '{print $2}')
    FO2_TIME=$(echo "$FO2_OUT" | grep "% Time elapsed:" | head -n 1 | awk '{print $4}')
    if [ -z "$FO2_STATUS" ]; then
        if echo "$FO2_OUT" | grep -qE "cannot open file|Parse error|Fatal error"; then
            FO2_STATUS="Error"
            FO2_TIME="0"
        else
            FO2_STATUS="Timeout"
            FO2_TIME="$TIME_LIMIT"
        fi
    fi

    echo "$fname,$CLASSIC_STATUS,$CLASSIC_TIME,$FO2_STATUS,$FO2_TIME"
}

export -f run_single_test

find "$DATASET_DIR" -type f \( -name "*.p" -o -name "*.tptp" \) | xargs -P "$JOBS" -I {} bash -c 'run_single_test "$@"' _ {} >> "$CSV_FILE"

echo "--------------------------------------------------"
echo "Benchmark completato!"
echo "Risultati salvati in $CSV_FILE"
echo "--------------------------------------------------"
echo "RIEPILOGO RISULTATI:"
echo "Totale Test Eseguiti: $(tail -n +2 "$CSV_FILE" | wc -l)"
echo "Classic Solved (Satisfiable/Unsatisfiable): $(grep -E "Satisfiable|Unsatisfiable" "$CSV_FILE" | awk -F',' '$2 ~ /Satisfiable|Unsatisfiable/' | wc -l)"
echo "FO2 Solved (Satisfiable/Unsatisfiable):     $(grep -E "Satisfiable|Unsatisfiable" "$CSV_FILE" | awk -F',' '$4 ~ /Satisfiable|Unsatisfiable/' | wc -l)"
echo "=================================================="
