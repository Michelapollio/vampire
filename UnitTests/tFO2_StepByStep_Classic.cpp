#include "Debug/Assertion.hpp"
#include "Test/UnitTesting.hpp"
#include "Test/SyntaxSugar.hpp"

#include "Kernel/Problem.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/SortHelper.hpp"
#include "Lib/System.hpp"
#include "Lib/Timer.hpp"

#include "F02Fragment/RemoveEquality/RemoveEquality.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma1.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma2.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma3.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma4.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma5.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma6.hpp"
#include "F02Fragment/Classifier/Classifier.hpp"
#include "F02Fragment/FO2Logger.hpp"
#include "Shell/SimplifyFalseTrue.hpp"
#include "Saturation/ProvingHelper.hpp"
#include "Shell/UIHelper.hpp"

using namespace Kernel;
using namespace Test;
using namespace FO2Fragment;
using namespace Shell;

namespace {

UnitList* cloneUnitsForTest(UnitList* units) {
  UnitList* res = UnitList::empty();
  UnitList::Iterator it(units);
  while (it.hasNext()) {
    UnitList::push(it.next(), res);
  }
  return UnitList::reverse(res);
}

std::string solveWithClassic(UnitList* units) {
  UnitList* copyUnits = cloneUnitsForTest(units);
  Problem* copyPrb = new Problem(copyUnits);

  Problem* proved = doProving(copyPrb);

  std::string resStr = "Unknown";
  if (env.statistics->terminationReason == TerminationReason::REFUTATION) {
    resStr = "Unsatisfiable";
  } else if (env.statistics->terminationReason == TerminationReason::SATISFIABLE) {
    resStr = "Satisfiable";
  }

  delete proved;
  return resStr;
}

void verifyStepByStepEquisatisfiability(Problem& prb, const std::string& expectedInitialStatus)
{
  FO2Logger::setVerbosity(VerbosityLevel::QUIET);

  // Passo 0: Status iniziale col solutore classico
  std::string status0 = solveWithClassic(prb.units());
  if (expectedInitialStatus != "Any") {
    ASS_EQ(status0, expectedInitialStatus);
  }

  // Passo 1: Lemma 1
  Lemma1::applyLemma1(prb);
  std::string status1 = solveWithClassic(prb.units());
  ASS_EQ(status1, status0);

  // Passo 2: Lemma 2
  Lemma2::applyLemma2(prb);
  std::string status2 = solveWithClassic(prb.units());
  ASS_EQ(status2, status0);

  // Passo 3: Lemma 3
  Lemma3::applyLemma3(prb);
  std::string status3 = solveWithClassic(prb.units());
  ASS_EQ(status3, status0);

  // Passo 4: Lemma 4
  Lemmata::Lemma4::applyLemma4(prb);
  std::string status4 = solveWithClassic(prb.units());
  ASS_EQ(status4, status0);

  // Passo 5: Lemma 5
  Lemmata::Lemma5::applyLemma5(prb);
  std::string status5 = solveWithClassic(prb.units());
  ASS_EQ(status5, status0);

  // Passo 6: Lemma 6 + Proxy Eq
  Lemmata::Lemma6::applyLemma6(prb);
  UnitList::DelIterator eqProxyIt(prb.units());
  while (eqProxyIt.hasNext()) {
    Unit* u = eqProxyIt.next();
    if (!u->isClause()) {
      FormulaUnit* fu = static_cast<FormulaUnit*>(u);
      Formula* newForm = RemoveEquality::replaceEqInFormula(fu->formula());
      FormulaUnit* targetFu = fu;
      if (newForm != fu->formula()) {
        targetFu = new FormulaUnit(newForm, fu->inference());
      }
      FormulaUnit* simpFu = SimplifyFalseTrue::simplify(targetFu);
      eqProxyIt.replace(simpFu);
    }
  }
  std::string status6 = solveWithClassic(prb.units());
  ASS_EQ(status6, status0);
}

} // namespace

TEST_FUN(test_fo2_step_by_step_equisat_basic)
{
  Problem prb;
  const TermList sort = AtomicSort::defaultSort();

  TermList x = TermList::var(0);
  Formula *eq = new AtomicFormula(Literal::createEquality(true, x, x, sort));
  Formula *forallX = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), eq);

  Unit *u = new FormulaUnit(forallX, Inference(FromInput(UnitInputType::AXIOM)));
  UnitList::push(u, prb.units());

  verifyStepByStepEquisatisfiability(prb, "Satisfiable");
}

TEST_FUN(test_fo2_step_by_step_equisat_contradiction)
{
  Problem prb;
  const TermList sort = AtomicSort::defaultSort();

  unsigned pFunctor = env.signature->addFreshPredicate(1, "P");
  TermList x = TermList::var(0);
  Literal *pLitPos = Literal::create1(pFunctor, true, x);
  Literal *pLitNeg = Literal::create1(pFunctor, false, x);

  Formula *px = new AtomicFormula(pLitPos);
  Formula *notPx = new AtomicFormula(pLitNeg);

  Formula *forallPos = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), px);
  Formula *forallNeg = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), notPx);

  Unit *u1 = new FormulaUnit(forallPos, Inference(FromInput(UnitInputType::AXIOM)));
  Unit *u2 = new FormulaUnit(forallNeg, Inference(FromInput(UnitInputType::AXIOM)));

  UnitList::push(u1, prb.units());
  UnitList::push(u2, prb.units());

  verifyStepByStepEquisatisfiability(prb, "Unsatisfiable");
}
