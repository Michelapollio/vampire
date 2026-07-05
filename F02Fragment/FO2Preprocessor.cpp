#include "FO2Preprocessor.hpp"
#include "Flattening.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Term.hpp"
#include "FMB/ClauseFlattening.hpp"
#include "Lib/DHSet.hpp"
#include "Kernel/TermIterators.hpp"

using namespace Kernel;
using namespace Lib;


bool Preprocessor::containsAllVariables(const DHSet<unsigned>& vars, const DHSet<unsigned>& required)
{
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

bool Preprocessor::coversAllVariables(const Term* t, const DHSet<unsigned>& clauseVars)
{
  DHSet<unsigned> termVars;
  VariableIterator vit(t);
  while (vit.hasNext()) {
    TermList v = vit.next();
    termVars.insert(v.var());
  }
  return Preprocessor::containsAllVariables(termVars, clauseVars);
}

bool Preprocessor::hasMaximalLiteral(Clause* cl, const DHSet<unsigned>& clauseVars)
{
  for (int i = 0; i < (int)cl->length(); ++i) {
    Literal* lit = (*cl)[i];
    DHSet<unsigned> litVars;
    VariableIterator vit(lit);
    while (vit.hasNext()) {
      TermList v = vit.next();
      litVars.insert(v.var());
    }

    if (litVars.size() >= clauseVars.size() && Preprocessor::containsAllVariables(litVars, clauseVars)) {
      return true;
    }
  }

  return false;
}

bool Preprocessor::validateClause(Clause* cl, const char*& errorMessage)
{
  DHMap<unsigned, TermList, DefaultHash, DefaultHash2> varSorts;
  SortHelper::collectVariableSorts(cl, varSorts);
  if (varSorts.size() > 2) {
    errorMessage = "FO2Preprocessor: clause violates S2 variable bound.";
    return false;
  }

  DHSet<unsigned> clauseVars;
  VirtualIterator<unsigned> vit = cl->getVariableIterator();
  while (vit.hasNext()) {
    clauseVars.insert(vit.next());
  }

  for (int i = 0; i < (int)cl->length(); ++i) {
    Literal* lit = (*cl)[i];
    unsigned arity = lit->arity();
    for (unsigned a = 0; a < arity; ++a) {
      const TermList* tl = lit->nthArgument(a);
      if (tl->isTerm()) {
        const Term* t = tl->term();
        if (!t->isShallow()) {
          errorMessage = "FO2Preprocessor: clause contains nested function symbols (violates S2).";
          return false;
        }
        if (!t->ground() && !coversAllVariables(t, clauseVars)) {
          errorMessage = "FO2Preprocessor: clause has a non-covering functional term (violates S2 covering).";
          return false;
        }
      }
    }
  }

  if (!hasMaximalLiteral(cl, clauseVars)) {
    errorMessage = "FO2Preprocessor: clause has no maximal literal covering all variables (violates S2).";
    return false;
  }

  return true;
}



void Preprocessor::preprocess(Problem &prb)
{
  UnitList *units = prb.units();
  UnitList::DelIterator it(units);

  while (it.hasNext()) {
    Unit *u = it.next();

    if (!u->isClause()) {
      FormulaUnit *fu = static_cast<FormulaUnit *>(u);
      fu = Shell::NNF::nnf(fu);
      fu = Shell::Flattening::flatten(fu);
      fu = Shell::Skolem::skolemise(fu);

      if (fu != u) {
        it.replace(fu);
        u = fu;
      }
    }

    if (u->isClause()) {
      Clause *cl = static_cast<Clause *>(u);
      const char *errorMessage = nullptr;
      if (!validateClause(cl, errorMessage)) {
        std::cerr << errorMessage << std::endl;
        return;
      }
    }
  }
}

