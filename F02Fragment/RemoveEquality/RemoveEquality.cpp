#include "RemoveEquality.hpp"


#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/SortHelper.hpp"

#include "Lib/DHSet.hpp"
#include "Lib/List.hpp"

#include "Shell/DistinctGroupExpansion.hpp"
#include "Shell/Rectify.hpp"
#include "Shell/SimplifyFalseTrue.hpp"
#include "Shell/TPTPPrinter.hpp"

#include "Lib/Environment.hpp"

#include "Lemmata/Lemma1.hpp"
#include "Lemmata/Lemma2.hpp"
#include "Lemmata/Lemma3.hpp"
#include "Lemmata/Lemma4.hpp"
#include "Lemmata/Lemma5.hpp"
#include "Lemmata/Lemma6.hpp"
#include "F02Fragment/Classifier/Classifier.hpp"
#include "F02Fragment/FO2Logger.hpp"

#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

using namespace Kernel;
using namespace Shell;

namespace FO2Fragment {

void RemoveEquality::Lemma1Application(Kernel::Problem &prb){
  
  FO2Fragment::Lemma1::applyLemma1(prb);
}

void RemoveEquality::Lemma2Application(Kernel::Problem &prb)
{
  FO2Fragment::Lemma2::applyLemma2(prb);
}

void RemoveEquality::Lemma3Application(Kernel::Problem &prb)
{
  FO2Fragment::Lemma3::applyLemma3(prb);
}

void RemoveEquality::Lemma4Application(Kernel::Problem &prb)
{
  FO2Fragment::Lemmata::Lemma4::applyLemma4(prb);
}

void RemoveEquality::Lemma5Application(Kernel::Problem &prb)
{
  FO2Fragment::Lemmata::Lemma5::applyLemma5(prb);
}

void RemoveEquality::Lemma6Application(Kernel::Problem &prb)
{
  FO2Fragment::Lemmata::Lemma6::applyLemma6(prb);
}

namespace {

void dumpEqualityStage(Problem& prb, unsigned stage) {
  const char* directory = std::getenv("FO2_EQUALITY_TRACE_DIR");
  if (!directory) return;
  std::ofstream output(std::string(directory) + "/lemma_" + std::to_string(stage) + ".p");
  if (!output) throw std::runtime_error("Cannot write FO2_EQUALITY_TRACE_DIR snapshot");
  UnitList::Iterator it(prb.units());
  while (it.hasNext()) output << Shell::TPTPPrinter::toString(it.next()) << '\n';
}

Formula* normalizeEqualityFormula(Formula* f) {
  if (!f) return nullptr;
  switch (f->connective()) {
    case LITERAL: {
      Literal* lit = f->literal();
      if (lit && lit->isEquality()) {
        TermList arg0 = *lit->nthArgument(0);
        TermList arg1 = *lit->nthArgument(1);
        if (arg0.isVar() && arg1.isVar() && !lit->isTwoVarEquality()) {
          Literal* newLit = Literal::createEquality(lit->polarity(), arg0, arg1, AtomicSort::defaultSort());
          return new AtomicFormula(newLit);
        }
      }
      return f;
    }
    case NOT: {
      Formula* newArg = normalizeEqualityFormula(f->uarg());
      if (newArg != f->uarg()) return new NegatedFormula(newArg);
      return f;
    }
    case AND:
    case OR: {
      FormulaList* args = f->args();
      FormulaList* newArgs = FormulaList::empty();
      bool changed = false;
      FormulaList::Iterator it(args);
      while (it.hasNext()) {
        Formula* arg = it.next();
        Formula* newArg = normalizeEqualityFormula(arg);
        if (newArg != arg) changed = true;
        FormulaList::push(newArg, newArgs);
      }
      newArgs = FormulaList::reverse(newArgs);
      if (changed) return JunctionFormula::generalJunction(f->connective(), newArgs);
      FormulaList::destroy(newArgs);
      return f;
    }
    case IMP:
    case IFF:
    case XOR: {
      Formula* newLeft = normalizeEqualityFormula(f->left());
      Formula* newRight = normalizeEqualityFormula(f->right());
      if (newLeft != f->left() || newRight != f->right()) {
        return new BinaryFormula(f->connective(), newLeft, newRight);
      }
      return f;
    }
    case FORALL:
    case EXISTS: {
      Formula* newQarg = normalizeEqualityFormula(f->qarg());
      if (newQarg != f->qarg()) {
        return new QuantifiedFormula(f->connective(), f->vars(), newQarg);
      }
      return f;
    }
    default:
      return f;
  }
}

void normalizeEqualitiesInProblem(Problem& prb) {
  UnitList::DelIterator it(prb.units());
  while (it.hasNext()) {
    Unit* u = it.next();
    if (!u->isClause()) {
      FormulaUnit* fu = static_cast<FormulaUnit*>(u);
      Formula* norm = normalizeEqualityFormula(fu->formula());
      if (norm != fu->formula()) {
        FormulaUnit* newFu = new FormulaUnit(norm, fu->inference());
        it.replace(newFu);
      }
    }
  }
}

static unsigned s_eqProxySymbol = 0;

static Literal* replaceEqLiteral(Literal* lit) {
  if (!lit->isEquality()) return lit;
  if (s_eqProxySymbol == 0) {
    s_eqProxySymbol = env.signature->addFreshPredicate(2, "eq0");
  }
  TermList arg0 = *lit->nthArgument(0);
  TermList arg1 = *lit->nthArgument(1);
  TermList args[2] = {arg0, arg1};
  return Literal::create(s_eqProxySymbol, 2, lit->polarity(), args);
} 
} // namespace

Formula* RemoveEquality::replaceEqInFormula(Formula* f) {
  if (!f) return nullptr;
  switch (f->connective()) {
    case LITERAL:
      return new AtomicFormula(replaceEqLiteral(f->literal()));
    case NOT:
      return new NegatedFormula(replaceEqInFormula(f->uarg()));
    case AND:
    case OR: {
      FormulaList* newArgs = nullptr;
      FormulaList::Iterator it(f->args());
      while (it.hasNext()) {
        FormulaList::push(replaceEqInFormula(it.next()), newArgs);
      }
      return JunctionFormula::generalJunction(f->connective(), FormulaList::reverse(newArgs));
    }
    case IMP:
    case IFF:
    case XOR:
      return new BinaryFormula(f->connective(), replaceEqInFormula(f->left()), replaceEqInFormula(f->right()));
    case FORALL:
    case EXISTS: {
      QuantifiedFormula* qf = static_cast<QuantifiedFormula*>(f);
      return new QuantifiedFormula(qf->connective(), qf->varList(), replaceEqInFormula(qf->subformula()));
    }
    default:
      return f;
  }
}

void RemoveEquality::removeEquality(Kernel::Problem &prb)
{
  // Phase 0: Expand TPTP distinct object groups BEFORE equality removal.
  // e.g. "Apple" != "Microsoft" must be made explicit before Lemma1 runs,
  // otherwise the distinct-object semantics are lost after equality removal.
  if (env.signature->hasDistinctGroups()) {
    if (FO2Logger::showsDebug()) {
      FO2Logger::logDebug("RemoveEquality: expanding distinct groups before lemmata");
    }
    Shell::DistinctGroupExpansion(0 /* 0 = always expand */).apply(prb);
  }

  bool hasEq = false;
  bool isFO2 = Classifier::isFO2(prb.units(), hasEq);

  if (!isFO2) {
    FO2Logger::logPhase("Problem does NOT belong to the FO2 fragment (more than 2 free or quantified variables).");
    return;
  }

  if (!hasEq && !prb.hasEquality()) {
    FO2Logger::logPhase("Problem belongs to the FO2 fragment and does NOT contain equality. Skipping lemmata.");
    return;
  }

  normalizeEqualitiesInProblem(prb);
  Shell::Rectify::rectify(prb.units());

  FO2Logger::logPhase("Starting equality removal procedure (RemoveEquality)");

  FO2Logger::logLemma("BEFORE LEMMA 1", prb);
  dumpEqualityStage(prb, 0);
  Lemma1Application(prb);
  dumpEqualityStage(prb, 1);

  FO2Logger::logLemma("AFTER LEMMA 1", prb);
  Lemma2Application(prb);
  dumpEqualityStage(prb, 2);

  FO2Logger::logLemma("AFTER LEMMA 2", prb);
  Lemma3Application(prb);
  dumpEqualityStage(prb, 3);

  FO2Logger::logLemma("AFTER LEMMA 3", prb);
  Lemma4Application(prb);
  dumpEqualityStage(prb, 4);

  FO2Logger::logLemma("AFTER LEMMA 4", prb);
  Lemma5Application(prb);
  dumpEqualityStage(prb, 5);

  FO2Logger::logLemma("AFTER LEMMA 5", prb);
  Lemma6Application(prb);
  dumpEqualityStage(prb, 6);

  FO2Logger::logLemma("AFTER LEMMA 6", prb);

  // Lemma 6 is expected to have eliminated every equality occurrence.
  // Keep the proxy conversion available for diagnostics/manual use, but do not
  // silently turn any missed equality into an uninterpreted predicate here.

  FO2Logger::logPhase("Equality removal completed");
}

void RemoveEquality::replaceEqualityWithProxy(Kernel::Problem &prb)
{
  UnitList::DelIterator eqProxyIt(prb.units());
  while (eqProxyIt.hasNext()) {
    Unit* u = eqProxyIt.next();
    if (!u->isClause()) {
      FormulaUnit* fu = static_cast<FormulaUnit*>(u);
      Formula* newForm = replaceEqInFormula(fu->formula());
      FormulaUnit* targetFu = fu;
      if (newForm != fu->formula()) {
        targetFu = new FormulaUnit(newForm, fu->inference());
      }
      FormulaUnit* simpFu = Shell::SimplifyFalseTrue::simplify(targetFu);
      eqProxyIt.replace(simpFu);
    }
  }

  if (s_eqProxySymbol == 0) {
    for (unsigned p = 1; p < env.signature->predicates(); ++p) {
      Signature::Symbol* sym = env.signature->getPredicate(p);
      if (sym && sym->name() == "eq0") {
        s_eqProxySymbol = p;
        break;
      }
    }
  }

}

} // namespace FO2Fragment
