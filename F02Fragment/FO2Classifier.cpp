#include "FO2Classifier.hpp"

#include "Kernel/Unit.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Clause.hpp"

#include "Forwards.hpp"

using namespace Kernel;

/**
 * Funzione principale che analizza la UnitList
 * restituita dal parser e verifica se l'intero problema 
 * è nel frammento F02. Efettua controllo su tutte le unità (formule o clausole)
 */
bool FO2Fragment::Classifier::isFO2(UnitList *ul, bool &hasEq) {
    UnitList::Iterator it(ul);

    // accumulatore per la presenza di uguaglianze su tutto il problema
    hasEq = false;

    while (it.hasNext()){
        Unit *u = it.next();

        // insieme delle variabili per l'unità corrente
        DHSet<unsigned> vars;
        bool ok = true;

        if(u->isClause()){
            ok = isFO2Clause(u->asClause(), vars, hasEq); // se non è una clausola F02, l'intero problema non è F02
        }
        else {
            ok = isFO2Formula(u->getFormula(), vars, hasEq);
        }

        if(!ok) return false; // se una unità non è FO2, l'intero problema non è FO2
    }
    return true;
}

/**
 * Analisi ricorsiva delle formule FOF
 * @param f: formula da analizzare
 * @param vars: insieme delle variabili quantificate finora
 * @param hasEq: flag che indica se è stata incontrata un'uguaglianza
 * @return true se la formula è nel frammento F02, false altrimenti
 */

 bool FO2Fragment::Classifier::isFO2Formula(Formula *f, DHSet<unsigned> &varSet, bool &hasEq) {
    switch (f->connective()){
        case AND:
        case OR: {
            FormulaList::Iterator it(f->args());
            while (it.hasNext()){
                if (!isFO2Formula(it.next(), varSet, hasEq)) {
                    return false; // se una delle formule argomento non è F02, la formula non è F02
                }
            }
            return true;
        }
        case FORALL:
        case EXISTS: {
            VSList::Iterator vit(f->vars());
            while (vit.hasNext()){
                varSet.insert(vit.next().first);
            }

            if (varSet.size() > 2) {
                return false;
            }

            return isFO2Formula(f->qarg(), varSet, hasEq);
        }
        case BOOL_TERM: {
            TermList ts = f->getBooleanTerm();
            if (!ts.isVar()) return false;
            varSet.insert(ts.var());
            return varSet.size() <= 2;
        }
        case NOT:
            return isFO2Formula(f->uarg(), varSet, hasEq);
        case IMP:
        case IFF:
            return isFO2Formula(f->left(), varSet, hasEq) && isFO2Formula(f->right(), varSet, hasEq);
        case LITERAL: {
            const Literal* lit = f->literal();  //chiamo literal() così da verificare che la formula è atomica, se non lo è -> asserzione
            if (!lit) return false;
            if (lit->isEquality()) hasEq = true;
            for (unsigned i = 0; i < (unsigned)lit->arity(); ++i) {
                const TermList* tl = lit->nthArgument(i);
                if (!tl) return false;
                if (tl->isVar()) {
                    varSet.insert(tl->var());
                    if (varSet.size() > 2) return false;
                } else if (tl->isTerm()) {
                    const Term* t = tl->term();
                    if (!t) return false;
                    if (t->arity() > 0) return false; // funzione non ammessa
                    // constante (arity==0) ammessa
                } else {
                    return false;
                }
            }
            return true;
        }
        case FALSE:
        case TRUE:
            return true; // costanti booleane sono ammesse

        default:
            return false;
    }
 }

 //Literal::isEquality()

 /**
  * Analisi delle clausole CNF
  * @param clause: clausola da analizzare
  * @param isEq: flag che indica se la clausola contiene un'uguaglianza
  * @return true se la clausola è nel frammento F02, false al
  */

    bool FO2Fragment::Classifier::isFO2Clause(Clause* clause, DHSet<unsigned> &vars, bool &hasEq) {

     for (unsigned i = 0; i < clause->length(); ++i) {
        vars.reset(); // resetto l'insieme delle variabili per ogni letterale
        const Literal* lit = (*clause)[i];
        if (!lit) return false;
        if (lit->isEquality()) {
            hasEq = true;
        }
        for (unsigned a = 0; a < (unsigned)lit->arity(); ++a) {
            const TermList* tl = lit->nthArgument(a);
            if (!tl) return false;
            if (tl->isVar()) {
                vars.insert(tl->var());
                if (vars.size() > 2) return false;
            } else if (tl->isTerm()) {
                const Term* t = tl->term();
                if (!t) return false;
                if (t->arity() > 0) return false; // funzione non ammessa
                // costante ammessa
            } else {
                return false;
            }
        }
     }

     return true;
    }