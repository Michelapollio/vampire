# FO2 Benchmark Datasets

Questa directory contiene i dataset di benchmark utilizzati per la valutazione e la validazione del solutore Vampire per il frammento a due variabili FO2 ($\text{FO}^2$).

I dataset sono stati suddivisi in due gruppi principali:

---

## 1. `generated/` — Dataset Generati Sinteticamente
Questa cartella racchiude i problemi generati automaticamente nel lavoro di tesi di **Manuel Borgia** (*"Generazione automatica di formule per frammenti decidibili"*).

Contiene i seguenti set di test con scaling parametrici e casi limite:
- `depthscaling/`: Formule con profondità sintattica e nidificazione crescenti.
- `equalityscaling/`: Formule con numero crescente di uguaglianze ed equazioni.
- `quantifierscaling/`: Scaling sul numero di quantificatori alternati.
- `satbuildscaling/`: Formule progettate per testare la scalabilità della costruzione del modello SAT.
- `timeout_cand/`: Problemi candidati difficili/timeout per valutare le prestazioni limite del solutore.
- `unknown_cand/`: Problemi il cui stato di soddisfacibilità iniziale era non noto.
- `vocabscaling/`: Formule con dimensione crescente del vocabolario (predicati e relazioni).

---

## 2. `tptp/` — Dataset Estratti dalla Libreria TPTP
Questa cartella racchiude i problemi estratti e classificati automaticamente dalla libreria standard **TPTP v8.2.0** tramite lo script `fo2_scripts/filter_tptp_fo2.sh`.

Contiene le seguenti sottocartelle:
- `fof_equality/`: Formule FOF (First-Order Formula) appartenenti a FO2 con uguaglianza.
- `fof_no_equality/`: Formule FOF appartenenti a FO2 senza uguaglianza.
- `cnf_equality/`: Clausole CNF appartenenti a FO2 con uguaglianza.
- `cnf_no_equality/`: Clausole CNF appartenenti a FO2 senza uguaglianza.

---

## Esecuzione dei Benchmark

Per eseguire i benchmark comparativi su una qualsiasi delle sottocartelle:

```bash
# Esempio su dataset TPTP (FOF con uguaglianza):
./fo2_scripts/run_comparative_benchmark.sh tests/fo2_datasets/tptp/fof_equality 1800 confronto_tptp_fof_eq 4

# Esempio su dataset generato (Equality Scaling):
./fo2_scripts/run_comparative_benchmark.sh tests/fo2_datasets/generated/equalityscaling 1800 confronto_gen_eqscaling 4
```
