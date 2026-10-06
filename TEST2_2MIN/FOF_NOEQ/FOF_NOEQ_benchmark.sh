#!/bin/bash

VAMPIRE="./build/vampire"
TPTP="/home/michela/Scaricati/TPTP-v9.2.1"
LIST_FILE="${1:-TEST2_2MIN/FOF_NOEQ/FOF_NOEQ_final_problems.txt}"
TIMEOUT="${2:-120}"
OUTPUT_CSV="TEST2_2MIN/FOF_NOEQ/FOF_NOEQ_results.csv"

if [ ! -f "$VAMPIRE" ]; then
    echo "Errore: $VAMPIRE binary non trovato!"
    exit 1
fi

if [ ! -f "$LIST_FILE" ]; then
    echo "Errore: File lista $LIST_FILE non trovato!"
    exit 1
fi

echo "FileName,FO2_Status,FO2_Time,Classic_Status,Classic_Time" > "$OUTPUT_CSV"

FILES=$(grep -v "^%" "$LIST_FILE" | grep -v '^$' | tr -d ' ')
TOTAL=$(echo "$FILES" | wc -l)

echo "=========================================================================="
echo " Benchmark su Frammento FO2 FOF SENZA Uguaglianza ($LIST_FILE)"
echo " Totale Problemi: $TOTAL | Timeout: ${TIMEOUT}s per problema"
echo "=========================================================================="

COUNT=0
FO2_SAT=0; FO2_UNSAT=0; FO2_TO=0; FO2_UNK=0; FO2_ERR=0
CLS_SAT=0; CLS_UNSAT=0; CLS_TO=0; CLS_UNK=0; CLS_ERR=0
MISMATCHES=0

for FILE in $FILES; do
    COUNT=$((COUNT + 1))
    prob_name="${FILE%.p}.p"
    domain="${prob_name:0:3}"
    problem_path="$TPTP/Problems/$domain/$prob_name"

    if [ ! -f "$problem_path" ]; then
        echo "[$COUNT/$TOTAL] [WARNING] File non trovato: $problem_path"
        continue
    fi

    # Run FO2 Mode
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

    # Run Classic Mode
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

    # Check mismatch
    if [ "$FO2_STAT" != "$CLS_STAT" ] && [ "$FO2_STAT" != "Timeout" ] && [ "$CLS_STAT" != "Timeout" ] && [ "$FO2_STAT" != "Unknown" ] && [ "$CLS_STAT" != "Unknown" ] && [ "$FO2_STAT" != "Error" ] && [ "$CLS_STAT" != "Error" ]; then
        MISMATCHES=$((MISMATCHES + 1))
        echo "[$COUNT/$TOTAL] [MISMATCH] $prob_name: FO2=$FO2_STAT ($FO2_TIME s) vs Classic=$CLS_STAT ($CLS_TIME s)"
    else
        echo "[$COUNT/$TOTAL] $prob_name: FO2=$FO2_STAT ($FO2_TIME s) | Classic=$CLS_STAT ($CLS_TIME s)"
    fi

    echo "$prob_name,$FO2_STAT,$FO2_TIME,$CLS_STAT,$CLS_TIME" >> "$OUTPUT_CSV"
done

echo "=========================================================================="
echo " BENCHMARK SUMMARY FOR L2 FOF NO EQUALITY"
echo " Total Problems: $TOTAL"
echo " FO2 Mode     : SAT=$FO2_SAT | UNSAT=$FO2_UNSAT | TIMEOUT=$FO2_TO | UNKNOWN=$FO2_UNK | ERR=$FO2_ERR"
echo " Classic Mode : SAT=$CLS_SAT | UNSAT=$CLS_UNSAT | TIMEOUT=$CLS_TO | UNKNOWN=$CLS_UNK | ERR=$CLS_ERR"
echo " Mismatches   : $MISMATCHES"
echo " Risultati salvati in: $OUTPUT_CSV"
echo "=========================================================================="
