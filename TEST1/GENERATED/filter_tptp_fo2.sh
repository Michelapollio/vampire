#!/usr/bin/env bash

VAMPIRE="./build/vampire"
INPUT_DIR="${1:-tests/tptp_raw}"
OUTPUT_DIR="${2:-tptp_classified}"

if [ ! -f "$VAMPIRE" ]; then
    echo "Error: Vampire binary not found at $VAMPIRE"
    exit 1
fi

if [ ! -d "$INPUT_DIR" ]; then
    echo "Error: Input directory $INPUT_DIR does not exist"
    exit 1
fi

DIR_FO2_EQ="$OUTPUT_DIR/fo2_with_equality"
DIR_FO2_NO_EQ="$OUTPUT_DIR/fo2_no_equality"
DIR_NON_FO2="$OUTPUT_DIR/non_fo2"

mkdir -p "$DIR_FO2_EQ" "$DIR_FO2_NO_EQ" "$DIR_NON_FO2"

echo "=========================================================================="
echo " Starting TPTP Problem Classification"
echo " Input Directory  : $INPUT_DIR"
echo " Output Directory : $OUTPUT_DIR"
echo "=========================================================================="

FILES=$(find "$INPUT_DIR" -type f -name "*.p" | sort)
TOTAL=$(echo "$FILES" | wc -l)

if [ "$TOTAL" -eq 0 ]; then
    echo "No .p files found in $INPUT_DIR"
    exit 0
fi

COUNT=0
CNT_FO2_EQ=0
CNT_FO2_NO_EQ=0
CNT_NON_FO2=0

for FILE in $FILES; do
    COUNT=$((COUNT + 1))
    FILENAME=$(basename "$FILE")

    OUT=$("$VAMPIRE" --mode fo2_classifier "$FILE" 2>&1)

    if echo "$OUT" | grep -q "^The problem is in FO2 fragment\."; then
        if echo "$OUT" | grep -q "FO2_HAS_EQUALITY: 1"; then
            cp "$FILE" "$DIR_FO2_EQ/$FILENAME"
            CNT_FO2_EQ=$((CNT_FO2_EQ + 1))
        else
            cp "$FILE" "$DIR_FO2_NO_EQ/$FILENAME"
            CNT_FO2_NO_EQ=$((CNT_FO2_NO_EQ + 1))
        fi
    else
        cp "$FILE" "$DIR_NON_FO2/$FILENAME"
        CNT_NON_FO2=$((CNT_NON_FO2 + 1))
    fi

    if [ $((COUNT % 50)) -eq 0 ] || [ "$COUNT" -eq "$TOTAL" ]; then
        echo "Processed $COUNT / $TOTAL files..."
    fi
done

echo ""
echo "=========================================================================="
echo " TPTP CLASSIFICATION SUMMARY"
echo "=========================================================================="
echo " Total Files Processed     : $TOTAL"
echo " FO2 (With Equality)      : $CNT_FO2_EQ -> $DIR_FO2_EQ"
echo " FO2 (Without Equality)   : $CNT_FO2_NO_EQ -> $DIR_FO2_NO_EQ"
echo " Non-FO2                   : $CNT_NON_FO2 -> $DIR_NON_FO2"
echo "=========================================================================="
