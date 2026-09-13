#include "FO2Preprocessor.hpp"

#include "Shell/Flattening.hpp" 
#include "Shell/NNF.hpp"
#include "Shell/Skolem.hpp"
#include "Shell/CNF.hpp"
#include "Shell/NewCNF.hpp"
#include "Shell/Rectify.hpp"
#include "Shell/Naming.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Term.hpp"
#include "FMB/ClauseFlattening.hpp"
#include "Lib/DHSet.hpp"
#include "Lib/Stack.hpp"
#include "Kernel/TermIterators.hpp"
#include "F02Fragment/RemoveEquality/RemoveEquality.hpp"
#include "F02Fragment/FO2Logger.hpp"

namespace FO2Preprocessor {
using namespace Kernel;
using namespace Lib;
using FO2Fragment::FO2Logger;

bool Preprocessor::containsAllVariables(const DHSet<unsigned> &vars, const DHSet<unsigned> &required)
{
  FO2Logger::logDebug("checking variable coverage: " + std::to_string(vars.size()) + " vs " + std::to_string(required.size()));
  if (vars.size() < required.size()) {
    return false;
  }

  for (DHSet<unsigned>::Iterator it(required); it.hasNext();) {
    if (!vars.contains(it.next())) {
      return false;
    }
  }

  return true;
}

bool Preprocessor::coversAllVariables(const Term *t, const DHSet<unsigned> &clauseVars)
{
  DHSet<unsigned> termVars;
  VariableIterator vit(t);
  while (vit.hasNext()) {
    TermList v = vit.next();
    termVars.insert(v.var());
  }
  return Preprocessor::containsAllVariables(termVars, clauseVars);
}

bool Preprocessor::validateClause(Clause *cl, const char *&errorMessage)
{
  DHMap<unsigned, TermList, DefaultHash, DefaultHash2> varSorts;
  SortHelper::collectVariableSorts(cl, varSorts);
  if (varSorts.size() > 2) {
    errorMessage = "FO2Preprocessor: clause violates S2 variable bound (more than 2 variables).";
    return false;
  }

  DHSet<unsigned> clauseVars;
  VirtualIterator<unsigned> vit = cl->getVariableIterator();
  while (vit.hasNext()) {
    clauseVars.insert(vit.next());
  }

  bool hasGroundLiteral = false;
  bool hasNonGroundLiteral = false;
  bool hasBivariateLiteral = false;

  for (int i = 0; i < (int)cl->length(); ++i) {
    Literal *lit = (*cl)[i];
    DHSet<unsigned> litVars;
    VariableIterator lvit(lit);
    while (lvit.hasNext()) {
      litVars.insert(lvit.next().var());
    }

    if (litVars.size() == 0) {
      hasGroundLiteral = true;
    } else {
      hasNonGroundLiteral = true;
    }

    if (clauseVars.size() == 2 && litVars.size() == 2) {
      hasBivariateLiteral = true;
    }

    unsigned arity = lit->arity();
    for (unsigned a = 0; a < arity; ++a) {
      const TermList *tl = lit->nthArgument(a);
      if (tl->isTerm()) {
        const Term *t = tl->term();
        if (!t->isShallow()) {
          errorMessage = "FO2Preprocessor: clause contains nested function symbols (violates S2).";
          return false;
        }
        if (!t->ground() && !Preprocessor::coversAllVariables(t, clauseVars)) {
          errorMessage = "FO2Preprocessor: clause has a non-covering functional term (violates S2 covering).";
          return false;
        }
      }
    }
  }

  if (hasGroundLiteral && hasNonGroundLiteral) {
    errorMessage = "FO2Preprocessor: clause containing ground literal must be entirely ground (violates S2 ground invariant).";
    return false;
  }

  if (clauseVars.size() == 2 && !hasBivariateLiteral) {
    errorMessage = "FO2Preprocessor: 2-variable clause must contain at least one literal with both variables (violates S2 covering).";
    return false;
  }

  return true;
}

bool Preprocessor::preprocess(Problem &prb)
{
  FO2Logger::logPhase("Starting FO2 Preprocessing");

  // Phase 1 & 2: NNF, Naming, Flattening, Skolemization and Clausification (CNF)
  UnitList::DelIterator it(prb.units());
  Stack<Clause*> clauses;
  Shell::NewCNF newCnf(0);
  Shell::Naming naming(1, false, false);

  while (it.hasNext()) {
    Unit *u = it.next();

    FO2Logger::logDebug("processing unit " + u->toString());

    if (!u->isClause()) {
      FormulaUnit *fu = static_cast<FormulaUnit *>(u);
      FO2Logger::logDebug("applying Rectify/Naming/NNF/flattening/skolemisation/clausification");
      fu = Shell::Rectify::rectify(fu);
      fu = Shell::NNF::nnf(fu);
      UnitList* defs = nullptr;
      fu = naming.apply(fu, defs);
      if (defs) {
        UnitList::Iterator defIt(defs);
        while (defIt.hasNext()) {
          it.insert(defIt.next());
        }
      }
      fu = Shell::Flattening::flatten(fu);
      fu = Shell::Skolem::skolemise(fu);

      clauses.reset();
      newCnf.clausify(fu, clauses);

      while (!clauses.isEmpty()) {
        Clause *cl = clauses.pop();
        it.insert(cl);
      }
      it.del();
    }
  }

  // Phase 3: Validation of S2 constraints on all resulting clauses
  bool valid = true;
  UnitList::Iterator uit(prb.units());
  while (uit.hasNext()) {
    Unit *u = uit.next();
    if (u->isClause()) {
      Clause *cl = static_cast<Clause *>(u);
      const char *errorMessage = nullptr;
      FO2Logger::logDebug("entering clause validation");
      if (!validateClause(cl, errorMessage)) {
        FO2Logger::logDebug("FO2Preprocessor Validation Warning: " + std::string(errorMessage));
        valid = false;
      }
    }
  }

  if (!valid) {
    FO2Logger::logDebug("FO2Preprocessor: Some pre-saturation clauses contain components that violate S2 constraints.");
  }

  FO2Logger::logLemma("PROBLEM AFTER PREPROCESSING", prb);
  FO2Logger::logPhase("FO2 Preprocessing completed");
  return valid;
}
} // namespace FO2Preprocessor
