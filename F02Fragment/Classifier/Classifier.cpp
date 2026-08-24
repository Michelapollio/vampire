#include "Classifier.hpp"

#include "Kernel/Unit.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Clause.hpp"

#include "Forwards.hpp"

using namespace Kernel;

/**
 * Main function that analyzes the UnitList returned by the parser
 * and checks whether the entire problem belongs to the FO2 fragment.
 * Checks all units (formulas or clauses).
 */
bool FO2Fragment::Classifier::isFO2(UnitList *ul, bool &hasEq) {
    UnitList::Iterator it(ul);

    hasEq = false;

    while (it.hasNext()){
        Unit *u = it.next();

        DHSet<unsigned> vars;
        bool ok = true;

        if(u->isClause()){
            ok = isFO2Clause(u->asClause(), vars, hasEq);
        }
        else {
            ok = isFO2Formula(u->getFormula(), vars, hasEq);
        }

        if(!ok) return false;
    }
    return true;
}

/**
 * Recursive analysis of FOF formulas
 * @param f Formula to analyze
 * @param varSet Set of quantified variables collected so far
 * @param hasEq Flag indicating if an equality literal was encountered
 * @return true if the formula is in the FO2 fragment, false otherwise
 */
 bool FO2Fragment::Classifier::isFO2Formula(Formula *f, DHSet<unsigned> &varSet, bool &hasEq) {
    switch (f->connective()){
        case AND:
        case OR: {
            FormulaList::Iterator it(f->args());
            while (it.hasNext()){
                if (!isFO2Formula(it.next(), varSet, hasEq)) {
                    return false;
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
            const Literal* lit = f->literal();
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
                    if (t->arity() > 0) return false;
                } else {
                    return false;
                }
            }
            return true;
        }
        case FALSE:
        case TRUE:
            return true;

        default:
            return false;
    }
 }

/**
 * Analysis of CNF clauses
 * @param clause Clause to analyze
 * @param vars Set of variables in the clause
 * @param hasEq Flag indicating if the clause contains an equality literal
 * @return true if the clause is in the FO2 fragment, false otherwise
 */
    bool FO2Fragment::Classifier::isFO2Clause(Clause* clause, DHSet<unsigned> &vars, bool &hasEq) {

     for (unsigned i = 0; i < clause->length(); ++i) {
        vars.reset();
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
                if (t->arity() > 0) return false;
            } else {
                return false;
            }
        }
     }

     return true;
    }