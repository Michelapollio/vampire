#!/bin/bash

# Script generico parallelizzato per classificare dataset TPTP con Vampire fo2_classifier

export TPTP="/home/michela/Scaricati/TPTP-v9.2.1"
export VAMPIRE_BIN="/home/michela/Scrivania/tirocinio/vampire/build/vampire"

CANDIDATES_FILE="$1"
OUTPUT_EQ_FILE="$2"
OUTPUT_NO_EQ_FILE="$3"

if [ -z "$CANDIDATES_FILE" ] || [ -z "$OUTPUT_EQ_FILE" ] || [ -z "$OUTPUT_NO_EQ_FILE" ]; then
    echo "Uso: $0 <candidates_file> <output_eq_file> <output_no_eq_file>"
    exit 1
fi

if [ ! -f "$VAMPIRE_BIN" ]; then
    echo "Errore: Eseguibile Vampire non trovato in: $VAMPIRE_BIN"
    exit 1
fi

if [ ! -f "$CANDIDATES_FILE" ]; then
    echo "Errore: File candidati non trovato in: $CANDIDATES_FILE"
    exit 1
fi

export OUTPUT_EQ_FILE OUTPUT_NO_EQ_FILE

> "$OUTPUT_EQ_FILE"
> "$OUTPUT_NO_EQ_FILE"

echo "=========================================================="
echo " Avvio classificazione PARALLELA con Vampire per: $CANDIDATES_FILE"
echo "=========================================================="

PROBLEMS=$(grep -v "^%" "$CANDIDATES_FILE" | awk '{print $1}' | tr -d ' ' | grep -v '^$')
TOTAL=$(echo "$PROBLEMS" | wc -l)

classify_one() {
    prob="$1"
    prob_name="${prob%.p}.p"
    domain="${prob_name:0:3}"
    problem_path="$TPTP/Problems/$domain/$prob_name"

    if [ -f "$problem_path" ]; then
        result=$("$VAMPIRE_BIN" --include "$TPTP" --mode fo2_classifier "$problem_path" 2>/dev/null)
        
        if echo "$result" | grep -q "FO2_HAS_EQUALITY: 1"; then
            echo "$prob_name" >> "$OUTPUT_EQ_FILE"
            echo "[FO2 WITH EQUALITY] $prob_name"
        elif echo "$result" | grep -q "FO2_HAS_EQUALITY: 0"; then
            echo "$prob_name" >> "$OUTPUT_NO_EQ_FILE"
            echo "[FO2 NO EQUALITY] $prob_name"
        else
            echo "[NOT FO2] $prob_name"
        fi
    else
        echo "[WARNING] File non trovato: $problem_path"
    fi
}

export -f classify_one

NPROC=$(nproc 2>/dev/null || echo 4)
echo "$PROBLEMS" | xargs -n 1 -P "$NPROC" -I {} bash -c 'classify_one "$@"' _ {}

# Ordina e rimuovi duplicati dai file di output
sort -u "$OUTPUT_EQ_FILE" -o "$OUTPUT_EQ_FILE"
sort -u "$OUTPUT_NO_EQ_FILE" -o "$OUTPUT_NO_EQ_FILE"

MATCH_EQ_COUNT=$(wc -l < "$OUTPUT_EQ_FILE")
MATCH_NO_EQ_COUNT=$(wc -l < "$OUTPUT_NO_EQ_FILE")

echo "=========================================================="
echo " Classificazione completata per $(basename "$CANDIDATES_FILE")!"
echo " Problemi FO2 CON uguaglianza salvati in : $OUTPUT_EQ_FILE ($MATCH_EQ_COUNT)"
echo " Problemi FO2 SENZA uguaglianza salvati in: $OUTPUT_NO_EQ_FILE ($MATCH_NO_EQ_COUNT)"
echo " Totale problemi FO2 identificati: $((MATCH_EQ_COUNT + MATCH_NO_EQ_COUNT)) / $TOTAL"
echo "=========================================================="
