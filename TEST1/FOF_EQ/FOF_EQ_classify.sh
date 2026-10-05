#!/bin/bash

# 1. Imposta i percorsi
export TPTP="/home/michela/Scaricati/TPTP-v9.2.1"
VAMPIRE_BIN="/home/michela/Scrivania/tirocinio/vampire/build/vampire"
CANDIDATES_FILE="TEST1.2min/L2_equality_final.txt"

OUTPUT_EQ_FILE="TEST1/FOF_EQ/FOF_EQ_final_problems.txt"
OUTPUT_NO_EQ_FILE="TEST1/FOF_NOEQ/FOF_NOEQ_final_problems.txt"

if [ ! -f "$VAMPIRE_BIN" ]; then
    echo "Errore: Eseguibile Vampire non trovato in: $VAMPIRE_BIN"
    exit 1
fi

if [ ! -f "$CANDIDATES_FILE" ]; then
    echo "Errore: File candidati non trovato in: $CANDIDATES_FILE"
    exit 1
fi

> "$OUTPUT_EQ_FILE"
> "$OUTPUT_NO_EQ_FILE"

echo "Avvio classificazione con Vampire su candidati FOF..."

# Estrai i nomi dei problemi
PROBLEMS=$(grep -v "^%" "$CANDIDATES_FILE" | awk '{print $1}' | tr -d ' ' | grep -v '^$')
TOTAL=$(echo "$PROBLEMS" | wc -l)
COUNT=0
MATCH_EQ_COUNT=0
MATCH_NO_EQ_COUNT=0

for prob in $PROBLEMS; do
    COUNT=$((COUNT + 1))
    
    # Assicura l'estensione .p se manca
    prob_name="${prob%.p}.p"
    domain="${prob_name:0:3}"
    problem_path="$TPTP/Problems/$domain/$prob_name"

    if [ -f "$problem_path" ]; then
        # Esegui Vampire includendo il percorso TPTP per risolvere eventuali include('Axioms/...')
        result=$("$VAMPIRE_BIN" --include "$TPTP" --mode fo2_classifier "$problem_path" 2>/dev/null)
        
        # Filtra i problemi nel frammento FO2
        if echo "$result" | grep -q "FO2_HAS_EQUALITY: 1"; then
            echo "$prob_name" >> "$OUTPUT_EQ_FILE"
            MATCH_EQ_COUNT=$((MATCH_EQ_COUNT + 1))
            echo "[$COUNT/$TOTAL] [FO2 WITH EQUALITY] $prob_name"
        elif echo "$result" | grep -q "FO2_HAS_EQUALITY: 0"; then
            echo "$prob_name" >> "$OUTPUT_NO_EQ_FILE"
            MATCH_NO_EQ_COUNT=$((MATCH_NO_EQ_COUNT + 1))
            echo "[$COUNT/$TOTAL] [FO2 NO EQUALITY] $prob_name"
        fi
    else
        echo "[$COUNT/$TOTAL] [WARNING] File non trovato: $problem_path"
    fi
done

echo "=========================================================="
echo "Classificazione completata!"
echo "Problemi FO2 CON uguaglianza salvati in : $OUTPUT_EQ_FILE ($MATCH_EQ_COUNT)"
echo "Problemi FO2 SENZA uguaglianza salvati in: $OUTPUT_NO_EQ_FILE ($MATCH_NO_EQ_COUNT)"
echo "Totale problemi selezionati: $((MATCH_EQ_COUNT + MATCH_NO_EQ_COUNT)) / $TOTAL"
echo "=========================================================="
