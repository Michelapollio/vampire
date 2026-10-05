#!/usr/bin/env bash

VAMPIRE="./build/vampire"
RAW_DIR="tests/tptp_raw/Problems"
OUT_DIR="tptp_classified"

FO2_NO_EQ_DIR="$OUT_DIR/fo2_without_equality"
FO2_WITH_EQ_DIR="$OUT_DIR/fo2_with_equality"
NON_FO2_DIR="$OUT_DIR/non_fo2"

rm -rf "$OUT_DIR"
mkdir -p "$FO2_NO_EQ_DIR"
mkdir -p "$FO2_WITH_EQ_DIR"
mkdir -p "$NON_FO2_DIR"

if [ ! -f "$VAMPIRE" ]; then
    echo "Error: $VAMPIRE binary not found!"
    exit 1
fi

if [ ! -d "$RAW_DIR" ]; then
    echo "Error: $RAW_DIR directory not found!"
    exit 1
fi

echo "=========================================================================="
echo " Classifying TPTP FOF & CNF problems in $RAW_DIR (Parallel Bash mode)"
echo "=========================================================================="

export VAMPIRE FO2_NO_EQ_DIR FO2_WITH_EQ_DIR NON_FO2_DIR

classify_one() {
    FILE="$1"
    FILENAME=$(basename "$FILE")
    OUT=$("$VAMPIRE" --mode fo2_classifier --include tests/tptp_raw "$FILE" 2>&1)

    if echo "$OUT" | grep -q "The problem is in FO2 fragment."; then
        if echo "$OUT" | grep -q "FO2_HAS_EQUALITY: 1"; then
            cp "$FILE" "$FO2_WITH_EQ_DIR/$FILENAME"
        else
            cp "$FILE" "$FO2_NO_EQ_DIR/$FILENAME"
        fi
    else
        cp "$FILE" "$NON_FO2_DIR/$FILENAME"
    fi
}

export -f classify_one

find "$RAW_DIR" -type f \( -name "*+*.p" -o -name "*-*.p" \) | sort | xargs -n 1 -P 8 -I {} bash -c 'classify_one "$@"' _ {}

TOTAL_NO_EQ=$(ls -1 "$FO2_NO_EQ_DIR" | wc -l)
TOTAL_WITH_EQ=$(ls -1 "$FO2_WITH_EQ_DIR" | wc -l)
TOTAL_NON_FO2=$(ls -1 "$NON_FO2_DIR" | wc -l)
TOTAL=$((TOTAL_NO_EQ + TOTAL_WITH_EQ + TOTAL_NON_FO2))

echo ""
echo "=========================================================================="
echo " CLASSIFICATION COMPLETE"
echo " Total FOF & CNF problems processed: $TOTAL"
echo " FO2 without equality: $TOTAL_NO_EQ (saved to $FO2_NO_EQ_DIR)"
echo " FO2 with equality   : $TOTAL_WITH_EQ (saved to $FO2_WITH_EQ_DIR)"
echo " Not in FO2          : $TOTAL_NON_FO2 (saved to $NON_FO2_DIR)"
echo "=========================================================================="
