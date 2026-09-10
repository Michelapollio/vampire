#include "FO2Preprocessor.hpp"

#include "Shell/Flattening.hpp" 
#include "Shell/NNF.hpp"
#include "Shell/Skolem.hpp"
#include "Shell/CNF.hpp"
#include "Shell/NewCNF.hpp"
#include "Shell/Rectify.hpp"
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

  for (int i = 0; i < (int)cl->length(); ++i) {
    Literal *lit = (*cl)[i];
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

  return true;
}

void Preprocessor::preprocess(Problem &prb)
{
  FO2Logger::logPhase("Starting FO2 Preprocessing");

  // Phase 1 & 2: NNF, Flattening, Skolemization and Clausification (CNF)
  UnitList::DelIterator it(prb.units());
  Stack<Clause*> clauses;
  Shell::NewCNF newCnf(0);

  while (it.hasNext()) {
    Unit *u = it.next();

    FO2Logger::logDebug("processing unit " + u->toString());

    if (!u->isClause()) {
      FormulaUnit *fu = static_cast<FormulaUnit *>(u);
      FO2Logger::logDebug("applying Rectify/NNF/flattening/skolemisation/clausification");
      fu = Shell::Rectify::rectify(fu);
      fu = Shell::NNF::nnf(fu);
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
    FO2Logger::logDebug("FO2Preprocessor: Some pre-saturation clauses contain components to be split via Splitting.");
  }

  FO2Logger::logLemma("PROBLEM AFTER PREPROCESSING", prb);
  FO2Logger::logPhase("FO2 Preprocessing completed");
}
} // namespace FO2Preprocessor
