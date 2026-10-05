#!/bin/bash

# Script per la classificazione dei candidati FOF SENZA UGUAGLIANZA nel frammento FO2

export TPTP="/home/michela/Scaricati/TPTP-v9.2.1"
VAMPIRE_BIN="/home/michela/Scrivania/tirocinio/vampire/build/vampire"
CANDIDATES_FILE="${1:-/home/michela/Scrivania/tirocinio/vampire/FOF_no_equality_candidates.txt}"

OUTPUT_NO_EQ_FILE="$HOME/L2_no_equality_final.txt"
LOCAL_NO_EQ_FILE="/home/michela/Scrivania/tirocinio/vampire/L2_fof_no_equality_final.txt"

if [ ! -f "$VAMPIRE_BIN" ]; then
    echo "Errore: Eseguibile Vampire non trovato in: $VAMPIRE_BIN"
    exit 1
fi

if [ ! -f "$CANDIDATES_FILE" ]; then
    echo "Errore: File candidati non trovato in: $CANDIDATES_FILE"
    exit 1
fi

> "$OUTPUT_NO_EQ_FILE"
> "$LOCAL_NO_EQ_FILE"

echo "=========================================================="
echo " Avvio classificazione FOF SENZA UGUAGLIANZA con Vampire"
echo " Candidati: $CANDIDATES_FILE"
echo "=========================================================="

PROBLEMS=$(grep -v "^%" "$CANDIDATES_FILE" | awk '{print $1}' | tr -d ' ' | grep -v '^$')
TOTAL=$(echo "$PROBLEMS" | wc -l)
COUNT=0
MATCH_NO_EQ_COUNT=0

for prob in $PROBLEMS; do
    COUNT=$((COUNT + 1))
    
    prob_name="${prob%.p}.p"
    domain="${prob_name:0:3}"
    problem_path="$TPTP/Problems/$domain/$prob_name"

    if [ -f "$problem_path" ]; then
        result=$("$VAMPIRE_BIN" --include "$TPTP" --mode fo2_classifier "$problem_path" 2>/dev/null)
        
        if echo "$result" | grep -q "FO2_HAS_EQUALITY: 0"; then
            echo "$prob_name" >> "$OUTPUT_NO_EQ_FILE"
            echo "$prob_name" >> "$LOCAL_NO_EQ_FILE"
            MATCH_NO_EQ_COUNT=$((MATCH_NO_EQ_COUNT + 1))
            echo "[$COUNT/$TOTAL] [FO2 NO EQUALITY] $prob_name"
        elif echo "$result" | grep -q "FO2_HAS_EQUALITY: 1"; then
            echo "[$COUNT/$TOTAL] [FO2 WITH EQUALITY - SKIPPED] $prob_name"
        else
            echo "[$COUNT/$TOTAL] [NOT FO2] $prob_name"
        fi
    else
        echo "[$COUNT/$TOTAL] [WARNING] File non trovato: $problem_path"
    fi
done

echo "=========================================================="
echo "Classificazione FOF SENZA UGUAGLIANZA completata!"
echo "Problemi FO2 SENZA uguaglianza salvati in: $LOCAL_NO_EQ_FILE ($MATCH_NO_EQ_COUNT / $TOTAL)"
echo "=========================================================="
