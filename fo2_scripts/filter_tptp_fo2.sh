#!/bin/bash

# Script Bash ottimizzato (Parallelo) per il filtraggio e la classificazione del benchmark TPTP per FO2
# Usage: ./scripts/filter_tptp_fo2.sh [INPUT_DIR] [OUTPUT_DIR] [VAMPIRE_EXEC] [JOBS]

INPUT_DIR="${1:-tests/tptp_raw/Problems}"
OUTPUT_DIR="${2:-tests/fo2_datasets/tptp}"
VAMPIRE_EXEC="${3:-./build/vampire}"
JOBS="${4:-$(nproc 2>/dev/null || echo 4)}"

MIN_BYTES=50
MAX_BYTES=10485760 # 10 MB

if [ ! -x "$VAMPIRE_EXEC" ]; then
    echo "Errore: Eseguibile Vampire non trovato in $VAMPIRE_EXEC"
    exit 1
fi

if [ ! -d "$INPUT_DIR" ]; then
    echo "Errore: Directory di input $INPUT_DIR non trovata."
    exit 1
fi

DIR_FOF_EQ="$OUTPUT_DIR/fof_equality"
DIR_FOF_NO_EQ="$OUTPUT_DIR/fof_no_equality"
DIR_CNF_EQ="$OUTPUT_DIR/cnf_equality"
DIR_CNF_NO_EQ="$OUTPUT_DIR/cnf_no_equality"

mkdir -p "$DIR_FOF_EQ" "$DIR_FOF_NO_EQ" "$DIR_CNF_EQ" "$DIR_CNF_NO_EQ"

echo "=================================================="
echo "FO2 TPTP Benchmark Filter & Classifier (Parallel Bash)"
echo "Scanning directory: $INPUT_DIR"
echo "Parallel jobs: $JOBS"
echo "=================================================="

export VAMPIRE_EXEC MIN_BYTES MAX_BYTES DIR_FOF_EQ DIR_FOF_NO_EQ DIR_CNF_EQ DIR_CNF_NO_EQ

process_file() {
    fpath="$1"
    
    # 1. Verifica dimensione file
    FSIZE=$(stat -c%s "$fpath" 2>/dev/null || stat -f%z "$fpath" 2>/dev/null)
    if [ -z "$FSIZE" ] || [ "$FSIZE" -lt "$MIN_BYTES" ] || [ "$FSIZE" -gt "$MAX_BYTES" ]; then
        return
    fi

    # 2. Verifica sintassi FOF / CNF (escludi TFF/THF)
    if grep -qE "tff\(|thf\(" "$fpath"; then
        return
    fi

    IS_FOF=0
    IS_CNF=0
    if grep -qE "fof\(" "$fpath"; then
        IS_FOF=1
    elif grep -qE "cnf\(" "$fpath"; then
        IS_CNF=1
    else
        return
    fi

    # 3. Escludi problemi puramente proposizionali (senza variabili/quantificatori)
    if [ "$IS_FOF" -eq 1 ]; then
        if ! grep -qE "!|\\?" "$fpath"; then
            return
        fi
    fi

    # 4. Verifica appartenenza a FO2 e classificazione semantica dell'uguaglianza tramite il Classifier C++ di Vampire (-m 2048)
    VAMP_OUT=$("$VAMPIRE_EXEC" --include "${TPTP_DIR:-tests/tptp_raw}" -m 2048 --mode fo2_classifier --time_limit 1 "$fpath" 2>&1)
    
    if ! echo "$VAMP_OUT" | grep -q "The problem is in FO2 fragment."; then
        return
    fi

    # 5. Rilevamento semantico dell'uguaglianza (basato su lit->isEquality() del C++ AST)
    HAS_EQ=0
    if echo "$VAMP_OUT" | grep -q "FO2_HAS_EQUALITY: 1"; then
        HAS_EQ=1
    fi

    # 6. Copia nella cartella corretta
    FNAME=$(basename "$fpath")
    if [ "$IS_FOF" -eq 1 ] && [ "$HAS_EQ" -eq 1 ]; then
        cp "$fpath" "$DIR_FOF_EQ/$FNAME"
    elif [ "$IS_FOF" -eq 1 ] && [ "$HAS_EQ" -eq 0 ]; then
        cp "$fpath" "$DIR_FOF_NO_EQ/$FNAME"
    elif [ "$IS_CNF" -eq 1 ] && [ "$HAS_EQ" -eq 1 ]; then
        cp "$fpath" "$DIR_CNF_EQ/$FNAME"
    else
        cp "$fpath" "$DIR_CNF_NO_EQ/$FNAME"
    fi
}

export -f process_file

# Esecuzione in parallelo con xargs
find "$INPUT_DIR" -type f \( -name "*.p" -o -name "*.tptp" \) | xargs -P "$JOBS" -I {} bash -c 'process_file "$@"' _ {}

echo "--------------------------------------------------"
echo "Filtraggio completato con successo!"
echo "Risultati generati sotto $OUTPUT_DIR:"
echo " - FOF con Uguaglianza:    $(ls -1 "$DIR_FOF_EQ" | wc -l) problemi"
echo " - FOF senza Uguaglianza: $(ls -1 "$DIR_FOF_NO_EQ" | wc -l) problemi"
echo " - CNF con Uguaglianza:    $(ls -1 "$DIR_CNF_EQ" | wc -l) problemi"
echo " - CNF senza Uguaglianza: $(ls -1 "$DIR_CNF_NO_EQ" | wc -l) problemi"
echo "=================================================="
