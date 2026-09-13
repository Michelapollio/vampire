/*
 * This file is part of the source code of the software program
 * Vampire. It is protected by applicable copyright laws.
 */

#include "Debug/Assertion.hpp"
#include "Test/UnitTesting.hpp"
#include "Test/SyntaxSugar.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/InferenceStore.hpp"

#include "F02Fragment/Resolution/FO2Resolution.hpp"
#include "F02Fragment/Resolution/FO2Inferences.hpp"
#include "F02Fragment/Resolution/FO2Solver.hpp"

using namespace Kernel;
using namespace Test;
using namespace FO2Fragment;

namespace {

Clause* createClause(std::initializer_list<Literal*> lits)
{
  return Clause::fromLiterals(lits, Inference(FromInput(UnitInputType::AXIOM)));
}

} // namespace

TEST_FUN(test_fo2_indexed_clause_and_sigma2_selection)
{
  TermList x = TermList::var(0);
  TermList y = TermList::var(1);

  unsigned pFunctor = env.signature->addFreshPredicate(1, "P");
  unsigned qFunctor = env.signature->addFreshPredicate(2, "Q");

  Literal *pLitX = Literal::create1(pFunctor, true, x);
  TermList qArgs[2] = {x, y};
  Literal *qLitXY = Literal::create(qFunctor, 2, true, qArgs);

  // Clause 1: P(x) (1 variable)
  Clause *cl1 = createClause({pLitX});
  IndexedClause icl1 = IndexedClause::fromClause(cl1);
  ASS_EQ(icl1.length(), 1u);
  ASS_EQ(icl1[0].index, 0u);

  // Clause 2: P(x) \lor Q(x,y) (2 variables)
  Clause *cl2 = createClause({pLitX, qLitXY});
  IndexedClause icl2 = IndexedClause::fromClause(cl2);
  ASS_EQ(icl2.length(), 2u);

  // Literal P(x) has 1 variable in a 2-variable clause -> index 0
  // Literal Q(x,y) has 2 variables in a 2-variable clause -> index 1
  ASS_EQ(icl2[0].index, 0u);
  ASS_EQ(icl2[1].index, 1u);

  // Check Sigma_2 selection on icl2
  auto selectedIndices = icl2.getSelectedLiteralIndices();
  ASS(!selectedIndices.empty());
}

TEST_FUN(test_fo2_inference_resolution)
{
  TermList x = TermList::var(0);
  TermList y = TermList::var(1);

  unsigned pFunctor = env.signature->addFreshPredicate(1, "P");
  unsigned qFunctor = env.signature->addFreshPredicate(2, "Q");
  unsigned rFunctor = env.signature->addFreshPredicate(1, "R");

  Literal *pLitX = Literal::create1(pFunctor, true, x);
  Literal *rLitY = Literal::create1(rFunctor, true, y);

  TermList qArgs[2] = {x, y};
  Literal *qLitPos = Literal::create(qFunctor, 2, true, qArgs);
  Literal *qLitNeg = Literal::create(qFunctor, 2, false, qArgs);

  // Clause A: Q(x,y) \lor P(x)  (Q(x,y) has index 1 and is selected by Sigma2)
  Clause *clA = createClause({qLitPos, pLitX});
  IndexedClause iclA = IndexedClause::fromClause(clA);

  // Clause B: ~Q(x,y) \lor R(y) (~Q(x,y) has index 1 and is selected by Sigma2)
  Clause *clB = createClause({qLitNeg, rLitY});
  IndexedClause iclB = IndexedClause::fromClause(clB);

  // Perform Resolution between selected lit 0 of iclA (Q(x,y)) and selected lit 0 of iclB (~Q(x,y))
  IndexedClause resolvent;
  bool resSuccess = FO2Inferences::resolve(iclA, 0, iclB, 0, resolvent);
  ASS(resSuccess);
  ASS_EQ(resolvent.length(), 2u);
}

TEST_FUN(test_fo2_inference_factoring)
{
  TermList x = TermList::var(0);
  unsigned pFunctor = env.signature->addFreshPredicate(1, "P");
  Literal *pLit1 = Literal::create1(pFunctor, true, x);
  Literal *pLit2 = Literal::create1(pFunctor, true, x);

  Clause *cl = createClause({pLit1, pLit2});
  IndexedClause icl = IndexedClause::fromClause(cl);

  IndexedClause factorResult;
  bool factSuccess = FO2Inferences::factor(icl, 0, 1, factorResult);
  ASS(factSuccess);
  ASS_EQ(factorResult.length(), 1u);
}

TEST_FUN(test_fo2_inference_subsumption)
{
  TermList x = TermList::var(0);
  TermList y = TermList::var(1);

  unsigned pFunctor = env.signature->addFreshPredicate(1, "P");
  unsigned qFunctor = env.signature->addFreshPredicate(2, "Q");

  Literal *pLitX = Literal::create1(pFunctor, true, x);
  TermList qArgs[2] = {x, y};
  Literal *qLitXY = Literal::create(qFunctor, 2, true, qArgs);

  // Subsuming clause: P(x)
  Clause *clSub = createClause({pLitX});
  IndexedClause iclSub = IndexedClause::fromClause(clSub);

  // Target clause: P(x) \lor Q(x,y)
  Clause *clTarget = createClause({pLitX, qLitXY});
  IndexedClause iclTarget = IndexedClause::fromClause(clTarget);

  bool isSubsumed = FO2Inferences::subsumes(iclSub, iclTarget);
  ASS(isSubsumed);
}

TEST_FUN(test_fo2_inference_split)
{
  TermList x = TermList::var(0);
  TermList y = TermList::var(1);

  unsigned pFunctor = env.signature->addFreshPredicate(1, "P");
  unsigned rFunctor = env.signature->addFreshPredicate(1, "R");

  Literal *pLitX = Literal::create1(pFunctor, true, x);
  Literal *rLitY = Literal::create1(rFunctor, true, y);

  // Clause with disjoint variables: P(x) \lor R(y)
  Clause *cl = createClause({pLitX, rLitY});
  IndexedClause icl = IndexedClause::fromClause(cl);

  IndexedClause outR1, outR2;
  bool splitSuccess = FO2Inferences::split(icl, outR1, outR2);
  ASS(splitSuccess);
  ASS_EQ(outR1.length(), 1u);
  ASS_EQ(outR2.length(), 1u);
}

TEST_FUN(test_fo2_solver_satisfiable)
{
  Problem prb;
  TermList x = TermList::var(0);
  unsigned pFunctor = env.signature->addFreshPredicate(1, "P");
  Literal *pLit = Literal::create1(pFunctor, true, x);

  Clause *cl = createClause({pLit});
  UnitList::push(cl, prb.units());

  FO2Result result = FO2Solver::solve(prb);
  ASS(result == FO2Result::SATISFIABLE);
}

TEST_FUN(test_fo2_solver_unsatisfiable)
{
  Problem prb;
  TermList x = TermList::var(0);
  unsigned pFunctor = env.signature->addFreshPredicate(1, "P");

  Literal *pLitPos = Literal::create1(pFunctor, true, x);
  Literal *pLitNeg = Literal::create1(pFunctor, false, x);

  Clause *cl1 = createClause({pLitPos});
  Clause *cl2 = createClause({pLitNeg});

  UnitList::push(cl1, prb.units());
  UnitList::push(cl2, prb.units());

  FO2Result result = FO2Solver::solve(prb);
  ASS(result == FO2Result::UNSATISFIABLE);
}
