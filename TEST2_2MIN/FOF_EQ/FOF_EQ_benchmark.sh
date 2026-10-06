#!/bin/bash

VAMPIRE="./build/vampire"
TPTP="/home/michela/Scaricati/TPTP-v9.2.1"
LIST_FILE="${1:-TEST2_2MIN/FOF_EQ/FOF_EQ_final_problems.txt}"
TIMEOUT="${2:-120}"
OUTPUT_CSV="TEST2_2MIN/FOF_EQ/FOF_EQ_results.csv"

if [ ! -f "$VAMPIRE" ]; then
    echo "Errore: $VAMPIRE binary non trovato!"
    exit 1
fi

if [ ! -f "$LIST_FILE" ]; then
    echo "Errore: File lista $LIST_FILE non trovato!"
    exit 1
fi

echo "FileName,FO2_Status,FO2_Time,Classic_Status,Classic_Time,FO2_RemoveEq_Status,FO2_RemoveEq_Time" > "$OUTPUT_CSV"

FILES=$(grep -v "^%" "$LIST_FILE" | grep -v '^$' | tr -d ' ')
TOTAL=$(echo "$FILES" | wc -l)

echo "=========================================================================="
echo " Benchmark su Frammento FO2 CON Uguaglianza ($LIST_FILE)"
echo " Totale Problemi: $TOTAL | Timeout: ${TIMEOUT}s per problema"
echo " Modes: 1. --mode fo2 | 2. Classic Otter | 3. --mode fo2_remove_equality"
echo "=========================================================================="

COUNT=0
FO2_SAT=0; FO2_UNSAT=0; FO2_TO=0; FO2_UNK=0; FO2_ERR=0
CLS_SAT=0; CLS_UNSAT=0; CLS_TO=0; CLS_UNK=0; CLS_ERR=0
REQ_SAT=0; REQ_UNSAT=0; REQ_TO=0; REQ_UNK=0; REQ_ERR=0

for FILE in $FILES; do
    COUNT=$((COUNT + 1))
    prob_name="${FILE%.p}.p"
    domain="${prob_name:0:3}"
    problem_path="$TPTP/Problems/$domain/$prob_name"

    if [ ! -f "$problem_path" ]; then
        if [ -f "tptp_classified/fo2_with_equality/$prob_name" ]; then
            problem_path="tptp_classified/fo2_with_equality/$prob_name"
        else
            echo "[$COUNT/$TOTAL] [WARNING] File non trovato: $problem_path"
            continue
        fi
    fi

    # 1. Run FO2 Mode
    START_T=$(date +%s.%N)
    FO2_OUT=$("$VAMPIRE" -t "$TIMEOUT" -m 4096 --include "$TPTP" --mode fo2 "$problem_path" 2>&1)
    END_T=$(date +%s.%N)
    FO2_TIME=$(echo "$END_T - $START_T" | bc -l | xargs printf "%.3f")

    if echo "$FO2_OUT" | grep -E -q "% SZS status (Satisfiable|CounterSatisfiable)"; then
        FO2_STAT="Satisfiable"
        FO2_SAT=$((FO2_SAT + 1))
    elif echo "$FO2_OUT" | grep -E -q "% SZS status (Unsatisfiable|Theorem|ContradictoryAxioms)|Termination reason: Refutation[[:space:]\r]*$"; then
        FO2_STAT="Unsatisfiable"
        FO2_UNSAT=$((FO2_UNSAT + 1))
    elif echo "$FO2_OUT" | grep -E -q "% SZS status Timeout|Time limit reached|Termination reason: (Time limit|TIME_LIMIT)"; then
        FO2_STAT="Timeout"
        FO2_TO=$((FO2_TO + 1))
    elif echo "$FO2_OUT" | grep -E -q "% SZS status Unknown|Termination reason: (Unknown|UNKNOWN)"; then
        FO2_STAT="Unknown"
        FO2_UNK=$((FO2_UNK + 1))
    else
        FO2_STAT="Error"
        FO2_ERR=$((FO2_ERR + 1))
    fi

    # 2. Run Classic Mode
    START_T=$(date +%s.%N)
    CLS_OUT=$("$VAMPIRE" -t "$TIMEOUT" -m 4096 --include "$TPTP" --saturation_algorithm otter -av off -updr off -bs on -fsr off "$problem_path" 2>&1)
    END_T=$(date +%s.%N)
    CLS_TIME=$(echo "$END_T - $START_T" | bc -l | xargs printf "%.3f")

    if echo "$CLS_OUT" | grep -E -q "% SZS status (Satisfiable|CounterSatisfiable)"; then
        CLS_STAT="Satisfiable"
        CLS_SAT=$((CLS_SAT + 1))
    elif echo "$CLS_OUT" | grep -E -q "% SZS status (Unsatisfiable|Theorem|ContradictoryAxioms)|Termination reason: Refutation[[:space:]\r]*$"; then
        CLS_STAT="Unsatisfiable"
        CLS_UNSAT=$((CLS_UNSAT + 1))
    elif echo "$CLS_OUT" | grep -E -q "% SZS status Timeout|Time limit reached|Termination reason: (Time limit|TIME_LIMIT)"; then
        CLS_STAT="Timeout"
        CLS_TO=$((CLS_TO + 1))
    elif echo "$CLS_OUT" | grep -E -q "% SZS status Unknown|Termination reason: (Unknown|UNKNOWN)"; then
        CLS_STAT="Unknown"
        CLS_UNK=$((CLS_UNK + 1))
    else
        CLS_STAT="Error"
        CLS_ERR=$((CLS_ERR + 1))
    fi

    # 3. Run FO2_RemoveEq Mode
    START_T=$(date +%s.%N)
    REQ_OUT=$("$VAMPIRE" -t "$TIMEOUT" -m 4096 --include "$TPTP" --mode fo2_remove_equality --saturation_algorithm otter -av off -updr off -bs on -fsr off "$problem_path" 2>&1)
    END_T=$(date +%s.%N)
    REQ_TIME=$(echo "$END_T - $START_T" | bc -l | xargs printf "%.3f")

    if echo "$REQ_OUT" | grep -E -q "% SZS status (Satisfiable|CounterSatisfiable)"; then
        REQ_STAT="Satisfiable"
        REQ_SAT=$((REQ_SAT + 1))
    elif echo "$REQ_OUT" | grep -E -q "% SZS status (Unsatisfiable|Theorem|ContradictoryAxioms)|Termination reason: Refutation[[:space:]\r]*$"; then
        REQ_STAT="Unsatisfiable"
        REQ_UNSAT=$((REQ_UNSAT + 1))
    elif echo "$REQ_OUT" | grep -E -q "% SZS status Timeout|Time limit reached|Termination reason: (Time limit|TIME_LIMIT)"; then
        REQ_STAT="Timeout"
        REQ_TO=$((REQ_TO + 1))
    elif echo "$REQ_OUT" | grep -E -q "% SZS status Unknown|Termination reason: (Unknown|UNKNOWN)"; then
        REQ_STAT="Unknown"
        REQ_UNK=$((REQ_UNK + 1))
    else
        REQ_STAT="Error"
        REQ_ERR=$((REQ_ERR + 1))
    fi

    echo "[$COUNT/$TOTAL] $prob_name: FO2=$FO2_STAT ($FO2_TIME s) | Classic=$CLS_STAT ($CLS_TIME s) | FO2_RemoveEq=$REQ_STAT ($REQ_TIME s)"
    echo "$prob_name,$FO2_STAT,$FO2_TIME,$CLS_STAT,$CLS_TIME,$REQ_STAT,$REQ_TIME" >> "$OUTPUT_CSV"
done

echo "=========================================================================="
echo " BENCHMARK SUMMARY FOR L2 EQUALITY (3 MODES)"
echo " Total Problems: $TOTAL"
echo " FO2 Mode          : SAT=$FO2_SAT | UNSAT=$FO2_UNSAT | TIMEOUT=$FO2_TO | UNKNOWN=$FO2_UNK | ERR=$FO2_ERR"
echo " Classic Mode      : SAT=$CLS_SAT | UNSAT=$CLS_UNSAT | TIMEOUT=$CLS_TO | UNKNOWN=$CLS_UNK | ERR=$CLS_ERR"
echo " FO2_RemoveEq Mode : SAT=$REQ_SAT | UNSAT=$REQ_UNSAT | TIMEOUT=$REQ_TO | UNKNOWN=$REQ_UNK | ERR=$REQ_ERR"
echo " Risultati salvati in: $OUTPUT_CSV"
echo "=========================================================================="
