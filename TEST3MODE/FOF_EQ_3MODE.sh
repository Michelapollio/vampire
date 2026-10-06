#!/bin/bash

# Resolution of base directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"
while [ ! -f "$REPO_ROOT/build/vampire" ] && [ "$REPO_ROOT" != "/" ]; do
    REPO_ROOT="$(dirname "$REPO_ROOT")"
done

VAMPIRE="${VAMPIRE_BIN:-$REPO_ROOT/build/vampire}"
TPTP="${TPTP_PATH:-/home/michela/Scaricati/TPTP-v9.2.1}"

LIST_FILE="${1:-}"
if [ -z "$LIST_FILE" ]; then
    if [ -f "$SCRIPT_DIR/FOF/FOF_EQ_final_problems.txt" ]; then
        LIST_FILE="$SCRIPT_DIR/FOF/FOF_EQ_final_problems.txt"
    elif [ -f "$SCRIPT_DIR/FOF_EQ_final_problems.txt" ]; then
        LIST_FILE="$SCRIPT_DIR/FOF_EQ_final_problems.txt"
    elif [ -f "TEST3MODE/FOF/FOF_EQ_final_problems.txt" ]; then
        LIST_FILE="TEST3MODE/FOF/FOF_EQ_final_problems.txt"
    else
        LIST_FILE="TEST1/L2_equality_final.txt"
    fi
fi

TIMEOUT="${2:-30}"

if [ -d "$SCRIPT_DIR/FOF" ]; then
    OUTPUT_CSV="$SCRIPT_DIR/FOF/FOF_EQ_3MODE_results.csv"
else
    OUTPUT_CSV="$SCRIPT_DIR/FOF_EQ_3MODE_results.csv"
fi

if [ ! -f "$VAMPIRE" ]; then
    echo "Errore: Eseguibile Vampire non trovato in $VAMPIRE!"
    exit 1
fi

if [ ! -f "$LIST_FILE" ]; then
    echo "Errore: File lista $LIST_FILE non trovato!"
    exit 1
fi

echo "FileName,FO2_Status,FO2_Time,Classic_Status,Classic_Time,FO2_RemoveEq_Vampire_Status,FO2_RemoveEq_Vampire_Time" > "$OUTPUT_CSV"

FILES=$(grep -v "^%" "$LIST_FILE" | grep -v '^$' | tr -d ' ')
TOTAL=$(echo "$FILES" | wc -l)

echo "=========================================================================="
echo " Benchmark su Frammento FO2 CON Uguaglianza (3 Modalità)"
echo " Lista Problemi: $LIST_FILE"
echo " Totale Problemi: $TOTAL | Timeout: ${TIMEOUT}s per modalità"
echo " 1. Mode FO2 (--mode fo2)"
echo " 2. Mode Classic Otter (--saturation_algorithm otter -av off -updr off -bs on -fsr off)"
echo " 3. Mode FO2_RemoveEquality + Vampire (--mode fo2_remove_equality)"
echo "=========================================================================="

classify_status() {
    local out="$1"
    if echo "$out" | grep -E -q "% SZS status (Satisfiable|CounterSatisfiable)|Termination reason: Satisfiable"; then
        echo "Satisfiable"
    elif echo "$out" | grep -E -q "% SZS status (Unsatisfiable|Theorem|ContradictoryAxioms)|Termination reason: Refutation[[:space:]\r]*$"; then
        echo "Unsatisfiable"
    elif echo "$out" | grep -E -q "% SZS status Unknown|Termination reason: UNKNOWN"; then
        echo "Unknown"
    elif echo "$out" | grep -E -q "% SZS status Timeout|Time limit reached|Termination reason: (Time limit|TIME_LIMIT|Memory limit|MEMORY_LIMIT)"; then
        echo "Timeout"
    else
        echo "Unknown"
    fi
}

COUNT=0
for FILE in $FILES; do
    COUNT=$((COUNT + 1))
    prob_name="${FILE%.p}.p"
    domain="${prob_name:0:3}"
    problem_path="$TPTP/Problems/$domain/$prob_name"

    if [ ! -f "$problem_path" ]; then
        echo "[$COUNT/$TOTAL] [WARNING] File non trovato: $problem_path"
        continue
    fi

    echo -e "\n[$COUNT/$TOTAL] Esecuzione su: $prob_name..."

    # 1. Mode FO2
    START_T=$(date +%s.%N)
    FO2_OUT=$("$VAMPIRE" -t "$TIMEOUT" -m 4096 --include "$TPTP" --mode fo2 "$problem_path" 2>&1)
    END_T=$(date +%s.%N)
    FO2_TIME=$(echo "$END_T - $START_T" | bc -l 2>/dev/null | xargs printf "%.3f" 2>/dev/null || echo "$TIMEOUT")
    FO2_STAT=$(classify_status "$FO2_OUT")
    if [ "$FO2_STAT" = "Timeout" ]; then
        FO2_TIME=$(printf "%.3f" "$TIMEOUT")
    fi
    echo "  -> 1. Mode FO2                      : $FO2_STAT ($FO2_TIME s)"

    # 2. Mode Classic Otter
    START_T=$(date +%s.%N)
    CLS_OUT=$("$VAMPIRE" -t "$TIMEOUT" -m 4096 --include "$TPTP" --saturation_algorithm otter -av off -updr off -bs on -fsr off "$problem_path" 2>&1)
    END_T=$(date +%s.%N)
    CLS_TIME=$(echo "$END_T - $START_T" | bc -l 2>/dev/null | xargs printf "%.3f" 2>/dev/null || echo "$TIMEOUT")
    CLS_STAT=$(classify_status "$CLS_OUT")
    if [ "$CLS_STAT" = "Timeout" ]; then
        CLS_TIME=$(printf "%.3f" "$TIMEOUT")
    fi
    echo "  -> 2. Mode Classic Otter            : $CLS_STAT ($CLS_TIME s)"

    # 3. Mode FO2_RemoveEquality + Vampire
    START_T=$(date +%s.%N)
    REQ_OUT=$("$VAMPIRE" -t "$TIMEOUT" -m 4096 --include "$TPTP" --mode fo2_remove_equality "$problem_path" 2>&1)
    END_T=$(date +%s.%N)
    REQ_TIME=$(echo "$END_T - $START_T" | bc -l 2>/dev/null | xargs printf "%.3f" 2>/dev/null || echo "$TIMEOUT")
    REQ_STAT=$(classify_status "$REQ_OUT")
    if [ "$REQ_STAT" = "Timeout" ]; then
        REQ_TIME=$(printf "%.3f" "$TIMEOUT")
    fi
    echo "  -> 3. Mode FO2_RemoveEq + Vampire   : $REQ_STAT ($REQ_TIME s)"

    echo "$prob_name,$FO2_STAT,$FO2_TIME,$CLS_STAT,$CLS_TIME,$REQ_STAT,$REQ_TIME" >> "$OUTPUT_CSV"
done

echo "=========================================================================="
echo " Benchmark completato!"
echo " Risultati salvati in: $OUTPUT_CSV"
echo "=========================================================================="
