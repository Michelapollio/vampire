#include "Classifier.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Unit.hpp"

#include "Forwards.hpp"

using namespace Kernel;

/**
 * Analyze the parsed units and check whether the problem belongs to FO2.
 * The problem may contain formulas, clauses, or both.
 */
bool FO2Fragment::Classifier::isFO2(UnitList *ul, bool &hasEq) {
  UnitList::Iterator it(ul);
  hasEq = false;

  while (it.hasNext()) {
    Unit *unit = it.next();
    DHSet<unsigned> vars;

    bool ok;
    if (unit->isClause()) {
      ok = isFO2Clause(unit->asClause(), vars, hasEq);
    } else {
      ok = isFO2Formula(unit->getFormula(), vars, hasEq);
    }

    if (!ok) {
      return false;
    }
  }
  return true;
}

/**
 * Analyze a formula recursively, collecting the free variables of each
 * subformula and checking that no subformula requires more than two variables.
 */
bool FO2Fragment::Classifier::isFO2Formula(Formula *f, DHSet<unsigned> &varSet, bool &hasEq) {
  // Bound variables belong only to their body, so independent branches may
  // reuse the same variable names.
  varSet.reset();

  switch (f->connective()) {
    case AND:
    case OR: {
      FormulaList::Iterator it(f->args());
      while (it.hasNext()) {
        DHSet<unsigned> childVars;
        if (!isFO2Formula(it.next(), childVars, hasEq)) {
          return false;
        }
        varSet.loadFromIterator(childVars.iterator());
        if (varSet.size() > 2) {
          return false;
        }
      }
      return true;
    }
    case FORALL:
    case EXISTS: {
      if (!isFO2Formula(f->qarg(), varSet, hasEq)) {
        return false;
      }
      VSList::Iterator it(f->vars());
      while (it.hasNext()) {
        varSet.remove(it.next().first);
      }
      return true;
    }
    case BOOL_TERM: {
      TermList term = f->getBooleanTerm();
      if (!term.isVar()) {
        return false;
      }
      varSet.insert(term.var());
      return varSet.size() <= 2;
    }
    case NOT:
      return isFO2Formula(f->uarg(), varSet, hasEq);
    case IMP:
    case IFF: {
      DHSet<unsigned> rightVars;
      if (!isFO2Formula(f->left(), varSet, hasEq) ||
          !isFO2Formula(f->right(), rightVars, hasEq)) {
        return false;
      }
      varSet.loadFromIterator(rightVars.iterator());
      return varSet.size() <= 2;
    }
    case LITERAL: {
      const Literal* literal = f->literal();
      if (!literal) {
        return false;
      }
      if (literal->isEquality()) {
        hasEq = true;
      } else if (literal->arity() > 2) {
        return false;
      }

      for (unsigned i = 0; i < static_cast<unsigned>(literal->arity()); ++i) {
        const TermList* term = literal->nthArgument(i);
        if (!term) {
          return false;
        }
        if (term->isVar()) {
          varSet.insert(term->var());
          if (varSet.size() > 2) {
            return false;
          }
        } else if (term->isTerm()) {
          const Term* function = term->term();
          if (!function || function->arity() > 0) {
            return false;
          }
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
 * Analyze a clause, collecting its variables and checking its literals.
 */
bool FO2Fragment::Classifier::isFO2Clause(Clause* clause, DHSet<unsigned> &vars, bool &hasEq) {
  vars.reset();
  for (unsigned i = 0; i < clause->length(); ++i) {
    const Literal* literal = (*clause)[i];
    if (!literal) {
      return false;
    }
    if (literal->isEquality()) {
      hasEq = true;
    } else if (literal->arity() > 2) {
      return false;
    }

    for (unsigned arg = 0; arg < static_cast<unsigned>(literal->arity()); ++arg) {
      const TermList* term = literal->nthArgument(arg);
      if (!term) {
        return false;
      }
      if (term->isVar()) {
        vars.insert(term->var());
        if (vars.size() > 2) {
          return false;
        }
      } else if (term->isTerm()) {
        const Term* function = term->term();
        if (!function || function->arity() > 0) {
          return false;
        }
      } else {
        return false;
      }
    }
  }

  return true;
}
