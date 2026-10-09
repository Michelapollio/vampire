#include "FO2Preprocessor.hpp"

#include "Shell/Flattening.hpp" 
#include "Shell/NNF.hpp"
#include "Shell/NewCNF.hpp"
#include "Shell/Rectify.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Term.hpp"
#include "Lib/DHSet.hpp"
#include "Lib/Stack.hpp"
#include "Kernel/TermIterators.hpp"
#include "F02Fragment/FO2Logger.hpp"

#include "Shell/SimplifyFalseTrue.hpp"

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
  DHSet<unsigned> clauseVars;
  VirtualIterator<unsigned> vit = cl->getVariableIterator();
  while (vit.hasNext()) {
    clauseVars.insert(vit.next());
  }
  if (clauseVars.size() > 2) {
    errorMessage = "FO2Preprocessor S2 Definition 6 (1): clause contains more than 2 variables.";
    FO2Logger::logDebug("Clause failing Condition 6(1) (>2 vars): " + cl->toString());
    return false;
  }

  bool hasGroundLiteral = false;
  bool hasNonGroundLiteral = false;
  bool hasTwoVarLiteral = false;

  for (int i = 0; i < (int)cl->length(); ++i) {
    Literal *lit = (*cl)[i];
    DHSet<unsigned> litVars;
    VariableIterator lvit(lit);
    while (lvit.hasNext()) {
      litVars.insert(lvit.next().var());
    }

    if (litVars.size() == 0) {
      hasGroundLiteral = true;
    } else if (litVars.size() > 0) {
      hasNonGroundLiteral = true;
    }

    if (clauseVars.size() == 2 && litVars.size() == 2) {
      hasTwoVarLiteral = true;
    }

    // Definition 6 (1): c contains no nested function symbols
    unsigned arity = lit->arity();
    for (unsigned a = 0; a < arity; ++a) {
      const TermList *tl = lit->nthArgument(a);
      if (tl->isTerm()) {
        const Term *t = tl->term();
        if (!t->isShallow()) {
          errorMessage = "FO2Preprocessor S2 Definition 6 (1): clause contains nested function symbols.";
          return false;
        }
        // Definition 6 (3): Each functional term in c contains all variables occurring in c
        if (!t->ground() && !Preprocessor::coversAllVariables(t, clauseVars)) {
          errorMessage = "FO2Preprocessor S2 Definition 6 (3): functional term does not contain all variables of c.";
          return false;
        }
      }
    }
  }

  // Definition 6 (2): If c contains ground literals, then it is a ground clause.
  if (hasGroundLiteral && hasNonGroundLiteral) {
    errorMessage = "FO2Preprocessor S2 Definition 6 (2): clause containing ground literal is not entirely ground.";
    return false;
  }

  // Definition 6 (4): There is a literal in c that contains all variables of c
  if (clauseVars.size() == 2 && !hasTwoVarLiteral) {
    errorMessage = "FO2Preprocessor S2 Definition 6 (4): 2-variable clause has no literal containing all variables of c.";
    FO2Logger::logDebug("Clause failing Condition 6(4): " + cl->toString());
    return false;
  }

  return true;
}

bool Preprocessor::preprocess(Problem &prb)
{
  FO2Logger::logPhase("Starting FO2 Preprocessing");

  // Phase 1 & 2: use Vampire's formula transformations and NewCNF clausifier.
  // NewCNF performs Skolemization while clausifying and introduces naming
  // predicates when needed, so no separate Naming/Skolem pass is required.
  UnitList::DelIterator it(prb.units());
  Stack<Clause*> clauses;
  Shell::NewCNF newCnf(1);
  Stack<Clause*> allClauses;

  while (it.hasNext()) {
    Unit *u = it.next();

    if (u->isClause()) {
      allClauses.push(static_cast<Clause*>(u));
      it.del();
      continue;
    }

    FormulaUnit *fu = static_cast<FormulaUnit *>(u);
    if (FO2Logger::showsDebug()) {
      FO2Logger::logDebug("processing unit " + fu->toString());
    }

    fu = Shell::SimplifyFalseTrue::simplify(fu);
    fu = Shell::NNF::nnf(fu);
    fu = Shell::SimplifyFalseTrue::simplify(fu);
    fu = Shell::Rectify::rectify(fu);

    if (fu->formula()->connective() == FALSE) {
      Clause* emptyCl = Clause::fromStack(Stack<Literal*>(), NonspecificInference1(InferenceRule::INPUT, fu));
      allClauses.push(emptyCl);
      it.del();
      continue;
    }
    if (fu->formula()->connective() == TRUE) {
      it.del();
      continue;
    }

    fu = Shell::Flattening::flatten(fu);
    clauses.reset();
    newCnf.clausify(fu, clauses);
    while (!clauses.isEmpty()) {
      allClauses.push(clauses.pop());
    }
    it.del();
  }

  while (!allClauses.isEmpty()) {
    UnitList::push(allClauses.pop(), prb.units());
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
    FO2Logger::logDebug("FO2Preprocessor: some clausified clauses need the article's splitting rule before S2 saturation.");
  } else {
    FO2Logger::logLemma("PROBLEM AFTER PREPROCESSING", prb);
    FO2Logger::logPhase("FO2 Preprocessing completed");
  }
  return valid;
}
} // namespace FO2Preprocessor
