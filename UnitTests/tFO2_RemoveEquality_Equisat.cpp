/*
 * This file is part of the source code of the software program
 * Vampire. It is protected by applicable copyright laws.
 */

#include "Debug/Assertion.hpp"
#include "Test/UnitTesting.hpp"
#include "Test/SyntaxSugar.hpp"

#include "Kernel/Problem.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/SortHelper.hpp"

#include "F02Fragment/RemoveEquality/RemoveEquality.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma1.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma2.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma3.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma4.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma5.hpp"
#include "F02Fragment/RemoveEquality/Lemmata/Lemma6.hpp"
#include "F02Fragment/Classifier/Classifier.hpp"
#include "F02Fragment/FO2Logger.hpp"

using namespace Kernel;
using namespace Test;
using namespace FO2Fragment;

namespace {

/**
 * @brief Helper function that applies RemoveEquality Lemmata 1 to 6 step-by-step
 * and verifies invariants and structural validity after each stage.
 */
void runStepByStepRemoveEquality(Problem& prb)
{
  bool hasEq = false;
  bool isFO2 = Classifier::isFO2(prb.units(), hasEq);
  ASS(isFO2);

  // Phase 0: Initial problem
  FO2Logger::logPhase("--- STEP 0: INITIAL PROBLEM ---");
  FO2Logger::logLemma("BEFORE LEMMA 1", prb);

  // Step 1: Lemma 1 Application
  FO2Logger::logPhase("--- STEP 1: APPLYING LEMMA 1 ---");
  Lemma1::applyLemma1(prb);
  FO2Logger::logLemma("AFTER LEMMA 1", prb);
  ASS(prb.units()); // Ensure problem retains units list

  // Step 2: Lemma 2 Application
  FO2Logger::logPhase("--- STEP 2: APPLYING LEMMA 2 ---");
  Lemma2::applyLemma2(prb);
  FO2Logger::logLemma("AFTER LEMMA 2", prb);
  ASS(prb.units());

  // Step 3: Lemma 3 Application
  FO2Logger::logPhase("--- STEP 3: APPLYING LEMMA 3 ---");
  Lemma3::applyLemma3(prb);
  FO2Logger::logLemma("AFTER LEMMA 3", prb);
  ASS(prb.units());

  // Step 4: Lemma 4 Application
  FO2Logger::logPhase("--- STEP 4: APPLYING LEMMA 4 ---");
  Lemmata::Lemma4::applyLemma4(prb);
  FO2Logger::logLemma("AFTER LEMMA 4", prb);
  ASS(prb.units());

  // Step 5: Lemma 5 Application
  FO2Logger::logPhase("--- STEP 5: APPLYING LEMMA 5 ---");
  Lemmata::Lemma5::applyLemma5(prb);
  FO2Logger::logLemma("AFTER LEMMA 5", prb);
  ASS(prb.units());

  // Step 6: Lemma 6 Application
  FO2Logger::logPhase("--- STEP 6: APPLYING LEMMA 6 ---");
  Lemmata::Lemma6::applyLemma6(prb);
  FO2Logger::logLemma("AFTER LEMMA 6", prb);
  ASS(prb.units());

  FO2Logger::logPhase("--- EQUALITY REMOVAL STEP-BY-STEP COMPLETE ---");
}

} // namespace

TEST_FUN(test_fo2_remove_equality_step_by_step_basic)
{
  Problem prb;
  const TermList sort = AtomicSort::defaultSort();

  // Create a simple FO2 formula with equality: \forall x (x = x)
  TermList x = TermList::var(0);
  Formula *eq = new AtomicFormula(Literal::createEquality(true, x, x, sort));
  Formula *forallX = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), eq);

  Unit *u = new FormulaUnit(forallX, Inference(FromInput(UnitInputType::AXIOM)));
  UnitList::push(u, prb.units());

  // Run step-by-step verification across all 6 lemmata
  runStepByStepRemoveEquality(prb);
}

TEST_FUN(test_fo2_remove_equality_step_by_step_constants)
{
  Problem prb;

  // Create a formula with a constant: P(c)
  unsigned cFunctor = env.signature->addFreshFunction(0, "c");
  env.signature->getFunction(cFunctor)->setType(OperatorType::getConstantsType(AtomicSort::defaultSort()));
  TermList cTerm = TermList(Term::create(cFunctor, 0, nullptr));

  unsigned pFunctor = env.signature->addFreshPredicate(1, "P");
  Literal *pLit = Literal::create1(pFunctor, true, cTerm);

  Formula *atomicP = new AtomicFormula(pLit);
  Unit *u = new FormulaUnit(atomicP, Inference(FromInput(UnitInputType::AXIOM)));
  UnitList::push(u, prb.units());

  // Run step-by-step verification
  runStepByStepRemoveEquality(prb);
}

TEST_FUN(test_fo2_remove_equality_step_by_step_nested_equality)
{
  Problem prb;
  const TermList sort = AtomicSort::defaultSort();

  // Create formula: \forall x \exists y (x = y)
  TermList x = TermList::var(0);
  TermList y = TermList::var(1);

  Formula *eqXY = new AtomicFormula(Literal::createEquality(true, x, y, sort));
  Formula *existsY = new QuantifiedFormula(EXISTS, VSList::singleton({1u, sort}), eqXY);
  Formula *forallX = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), existsY);

  Unit *u = new FormulaUnit(forallX, Inference(FromInput(UnitInputType::AXIOM)));
  UnitList::push(u, prb.units());

  // Run step-by-step verification
  runStepByStepRemoveEquality(prb);
}
