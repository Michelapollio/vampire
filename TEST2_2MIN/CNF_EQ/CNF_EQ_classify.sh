#!/bin/bash

# 1. Imposta i percorsi
export TPTP="/home/michela/Scaricati/TPTP-v9.2.1"
VAMPIRE_BIN="/home/michela/Scrivania/tirocinio/vampire/build/vampire"
CANDIDATES_FILE="$HOME/CNF_candidates.txt"

OUTPUT_EQ_FILE="$HOME/L2_cnf_equality_final.txt"
OUTPUT_NO_EQ_FILE="$HOME/L2_cnf_no_equality_final.txt"
LOCAL_EQ_FILE="/home/michela/Scrivania/tirocinio/vampire/L2_cnf_equality_final.txt"
LOCAL_NO_EQ_FILE="/home/michela/Scrivania/tirocinio/vampire/L2_cnf_no_equality_final.txt"

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

echo "Avvio classificazione con Vampire su candidati CNF..."

# Estrai i nomi dei problemi (escludendo le righe che iniziano con %)
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
        # Esegui Vampire includendo il percorso TPTP
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
        else
            echo "[$COUNT/$TOTAL] [NOT FO2] $prob_name"
        fi
    else
        echo "[$COUNT/$TOTAL] [WARNING] File non trovato: $problem_path"
    fi
done

# Copia i risultati anche nella cartella locale del tirocinio
cp "$OUTPUT_EQ_FILE" "$LOCAL_EQ_FILE"
cp "$OUTPUT_NO_EQ_FILE" "$LOCAL_NO_EQ_FILE"

echo "=========================================================="
echo "Classificazione CNF completata!"
echo "Problemi FO2 CON uguaglianza salvati in : $OUTPUT_EQ_FILE ($MATCH_EQ_COUNT)"
echo "Problemi FO2 SENZA uguaglianza salvati in: $OUTPUT_NO_EQ_FILE ($MATCH_NO_EQ_COUNT)"
echo "Copia locale salvata in : $LOCAL_EQ_FILE e $LOCAL_NO_EQ_FILE"
echo "Totale problemi selezionati: $((MATCH_EQ_COUNT + MATCH_NO_EQ_COUNT)) / $TOTAL"
echo "=========================================================="
