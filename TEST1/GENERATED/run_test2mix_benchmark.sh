#!/usr/bin/env bash

VAMPIRE="./build/vampire"
TARGET_DIR="tests/generated/test2mix/fo2_datasets"
TIMEOUT=${1:-30}
OUTPUT_CSV="test2mix_benchmark_results.csv"

if [ ! -f "$VAMPIRE" ]; then
    echo "Error: $VAMPIRE binary not found!"
    exit 1
fi

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Target directory $TARGET_DIR not found!"
    exit 1
fi

echo "Dataset,FileName,FO2_Status,FO2_Time,Classic_Status,Classic_Time" > "$OUTPUT_CSV"

echo "=========================================================================="
echo " Starting benchmark on $TARGET_DIR (Timeout=${TIMEOUT}s)"
echo "=========================================================================="

FILES=$(find "$TARGET_DIR" -type f -name "*.p" | sort)
TOTAL=$(echo "$FILES" | wc -l)
COUNT=0

FO2_SAT=0
FO2_UNSAT=0
FO2_TO=0
FO2_ERR=0
FO2_TIME_SUM=0

CLS_SAT=0
CLS_UNSAT=0
CLS_TO=0
CLS_ERR=0
CLS_TIME_SUM=0

MISMATCHES=0

for FILE in $FILES; do
    COUNT=$((COUNT + 1))
    FILENAME=$(basename "$FILE")
    DATASET=$(basename "$(dirname "$FILE")")

    # Run FO2 Mode
    START_T=$(date +%s.%N)
    FO2_OUT=$("$VAMPIRE" -t "$TIMEOUT" -m 4096 --mode fo2 "$FILE" 2>&1)
    END_T=$(date +%s.%N)
    FO2_TIME=$(echo "$END_T - $START_T" | bc -l | xargs printf "%.3f")

    if echo "$FO2_OUT" | grep -E -q "% SZS status (Satisfiable|CounterSatisfiable)"; then
        FO2_STAT="Satisfiable"
        FO2_SAT=$((FO2_SAT + 1))
    elif echo "$FO2_OUT" | grep -E -q "% SZS status (Unsatisfiable|Theorem)|Termination reason: Refutation[[:space:]\r]*$"; then
        FO2_STAT="Unsatisfiable"
        FO2_UNSAT=$((FO2_UNSAT + 1))
    elif echo "$FO2_OUT" | grep -E -q "% SZS status (Timeout)|Time limit reached|Termination reason: (Time limit|TIME_LIMIT|Memory limit|MEMORY_LIMIT)"; then
        FO2_STAT="Timeout"
        FO2_TO=$((FO2_TO + 1))
    else
        FO2_STAT="Unknown"
        FO2_UNK=$((FO2_UNK + 1))
    fi
    FO2_TIME_SUM=$(echo "$FO2_TIME_SUM + $FO2_TIME" | bc -l)

    # Run Classic Mode
    START_T=$(date +%s.%N)
    CLS_OUT=$("$VAMPIRE" -t "$TIMEOUT" -m 4096 --saturation_algorithm otter -av off -updr off -bs on -fsr off "$FILE" 2>&1)
    END_T=$(date +%s.%N)
    CLS_TIME=$(echo "$END_T - $START_T" | bc -l | xargs printf "%.3f")

    if echo "$CLS_OUT" | grep -E -q "% SZS status (Satisfiable|CounterSatisfiable)"; then
        CLS_STAT="Satisfiable"
        CLS_SAT=$((CLS_SAT + 1))
    elif echo "$CLS_OUT" | grep -E -q "% SZS status (Unsatisfiable|Theorem)|Termination reason: Refutation[[:space:]\r]*$"; then
        CLS_STAT="Unsatisfiable"
        CLS_UNSAT=$((CLS_UNSAT + 1))
    elif echo "$CLS_OUT" | grep -E -q "% SZS status (Timeout)|Time limit reached|Termination reason: (Time limit|TIME_LIMIT|Memory limit|MEMORY_LIMIT)"; then
        CLS_STAT="Timeout"
        CLS_TO=$((CLS_TO + 1))
    else
        CLS_STAT="Unknown"
        CLS_UNK=$((CLS_UNK + 1))
    fi
    CLS_TIME_SUM=$(echo "$CLS_TIME_SUM + $CLS_TIME" | bc -l)

    # Check mismatch
    if [ "$FO2_STAT" != "$CLS_STAT" ] && [ "$FO2_STAT" != "Timeout" ] && [ "$CLS_STAT" != "Timeout" ] && [ "$FO2_STAT" != "Error" ] && [ "$CLS_STAT" != "Error" ]; then
        MISMATCHES=$((MISMATCHES + 1))
        echo " [MISMATCH] ($DATASET) $FILENAME: FO2=$FO2_STAT ($FO2_TIME s) vs Classic=$CLS_STAT ($CLS_TIME s)"
    fi

    if [[ "$FILENAME" == *","* ]]; then
        echo "$DATASET,\"$FILENAME\",$FO2_STAT,$FO2_TIME,$CLS_STAT,$CLS_TIME" >> "$OUTPUT_CSV"
    else
        echo "$DATASET,$FILENAME,$FO2_STAT,$FO2_TIME,$CLS_STAT,$CLS_TIME" >> "$OUTPUT_CSV"
    fi

    if [ $((COUNT % 20)) -eq 0 ] || [ "$COUNT" -eq "$TOTAL" ]; then
        echo "Processed $COUNT / $TOTAL problems..."
    fi
done

echo ""
echo "=========================================================================="
echo " BENCHMARK SUMMARY FOR $TARGET_DIR"
echo " Total Problems: $TOTAL"
echo "--------------------------------------------------------------------------"
FO2_AVG=$(echo "$FO2_TIME_SUM / $TOTAL" | bc -l | xargs printf "%.3f")
CLS_AVG=$(echo "$CLS_TIME_SUM / $TOTAL" | bc -l | xargs printf "%.3f")
echo " FO2 Mode     : SAT=$FO2_SAT | UNSAT=$FO2_UNSAT | TIMEOUT=$FO2_TO | ERR=$FO2_ERR | AvgTime=${FO2_AVG}s"
echo " Classic Mode : SAT=$CLS_SAT | UNSAT=$CLS_UNSAT | TIMEOUT=$CLS_TO | ERR=$CLS_ERR | AvgTime=${CLS_AVG}s"
echo " Equisatisfiability Mismatches: $MISMATCHES"
echo "=========================================================================="
echo "Results saved to $OUTPUT_CSV"
