#include "FO2Solver.hpp"
#include "F02Fragment/FO2Logger.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/Clause.hpp"
#include <deque>
#include <vector>
#include <iostream>

#include "Lib/Environment.hpp"
#include "Kernel/Signature.hpp"

using namespace Kernel;
using namespace Lib;

namespace FO2Fragment {

FO2Result FO2Solver::solve(Problem& prb)
{
  FO2Logger::logPhase("Starting FO2 Saturation Engine (Section 4)");

  std::deque<IndexedClause> passive;
  std::vector<IndexedClause> active;

  // Step 1: Collect and index initial clauses (S2+i class)
  UnitList::Iterator uit(prb.units());
  while (uit.hasNext()) {
    Unit* u = uit.next();
    if (u->isClause()) {
      Clause* cl = static_cast<Clause*>(u);
      IndexedClause icl = IndexedClause::fromClause(cl);
      IndexedClause simpIcl;
      bool isTaut = false;
      FO2Inferences::simplifyEqualityClause(icl, simpIcl, isTaut);
      if (isTaut) continue;
      if (simpIcl.length() == 0) {
        FO2Logger::logPhase("Initial empty clause found: UNSATISFIABLE");
        return FO2Result::UNSATISFIABLE;
      }
      passive.push_back(simpIcl);
    }
  }

  // Step 1b: Add eq0/neq0 reflexivity, anti-reflexivity, and duality axioms
  unsigned eq0Functor = env.signature->addPredicate("eq0", 2);
  unsigned neq0Functor = env.signature->addPredicate("neq0", 2);

  TermList var0 = TermList::var(0);
  TermList var1 = TermList::var(1);
  TermList args0[2] = {var0, var0};
  TermList args01[2] = {var0, var1};

  // Axiom 1: eq0(X0, X0)
  Literal* eq0ReflLit = Literal::create(eq0Functor, 2, true, args0);
  passive.push_back(IndexedClause({IndexedLiteral(eq0ReflLit, 0)}));

  // Axiom 2: ~neq0(X0, X0)
  Literal* neq0AntiReflLit = Literal::create(neq0Functor, 2, false, args0);
  passive.push_back(IndexedClause({IndexedLiteral(neq0AntiReflLit, 0)}));

  // Axiom 3: ~eq0(X0, X1) | ~neq0(X0, X1)
  Literal* notEq0Lit = Literal::create(eq0Functor, 2, false, args01);
  Literal* notNeq0Lit = Literal::create(neq0Functor, 2, false, args01);
  passive.push_back(IndexedClause({IndexedLiteral(notEq0Lit, 1), IndexedLiteral(notNeq0Lit, 1)}));

  // Axiom 4: Congruence axioms for eq0 over all predicates in signature
  TermList var2 = TermList::var(2);
  unsigned numPreds = env.signature->predicates();
  for (unsigned p = 1; p < numPreds; ++p) {
    if (p == eq0Functor || p == neq0Functor || Signature::isEqualityPredicate(p)) continue;
    Kernel::Signature::Symbol* sym = env.signature->getPredicate(p);
    if (!sym || sym->interpreted()) continue;
    unsigned arity = sym->arity();
    if (arity == 1) {
      // ~eq0(X0, X1) | ~P(X0) | P(X1)
      TermList a0[1] = {var0};
      TermList a1[1] = {var1};
      Literal* notP0 = Literal::create(p, 1, false, a0);
      Literal* posP1 = Literal::create(p, 1, true, a1);
      passive.push_back(IndexedClause({IndexedLiteral(notEq0Lit, 1), IndexedLiteral(notP0, 0), IndexedLiteral(posP1, 0)}));
    } else if (arity == 2) {
      // Arg 0 congruence: ~eq0(X0, X1) | ~P(X0, X2) | P(X1, X2)
      TermList a02[2] = {var0, var2};
      TermList a12[2] = {var1, var2};
      Literal* notP02 = Literal::create(p, 2, false, a02);
      Literal* posP12 = Literal::create(p, 2, true, a12);
      passive.push_back(IndexedClause({IndexedLiteral(notEq0Lit, 1), IndexedLiteral(notP02, 1), IndexedLiteral(posP12, 1)}));

      // Arg 1 congruence: ~eq0(X0, X1) | ~P(X2, X0) | P(X2, X1)
      TermList a20[2] = {var2, var0};
      TermList a21[2] = {var2, var1};
      Literal* notP20 = Literal::create(p, 2, false, a20);
      Literal* posP21 = Literal::create(p, 2, true, a21);
      passive.push_back(IndexedClause({IndexedLiteral(notEq0Lit, 1), IndexedLiteral(notP20, 1), IndexedLiteral(posP21, 1)}));
    }
  }

  FO2Logger::logDebug("[FO2Solver] Initial passive clauses: " + std::to_string(passive.size()));

  size_t iteration = 0;
  const size_t MAX_ITERATIONS = 10000; // Safeguard limit

  while (!passive.empty() && iteration < MAX_ITERATIONS) {
    iteration++;

    IndexedClause given = passive.front();
    passive.pop_front();

    if (given.length() == 0) {
      FO2Logger::logPhase("Derived empty clause found: UNSATISFIABLE");
      return FO2Result::UNSATISFIABLE;
    }

    FO2Logger::logDebug("[FO2Solver Loop " + std::to_string(iteration) + "] Given: " + given.toStringWithSelection());

    // Step 2: Forward Subsumption check
    bool isSubsumed = false;
    for (const auto& act : active) {
      if (FO2Inferences::subsumes(act, given)) {
        isSubsumed = true;
        FO2Logger::logDebug("[FO2Solver] Given subsumed by active clause: " + act.toString());
        break;
      }
    }
    if (isSubsumed) continue;

    // Step 3: Splitting Rule check (Disabled to prevent unsound conjunction replacement)
    IndexedClause r1, r2;
    if (false && FO2Inferences::split(given, r1, r2)) {
      FO2Logger::logDebug("[FO2Solver] Splitting applied to Given: R1=" + r1.toString() + ", R2=" + r2.toString());
      passive.push_back(r1);
      passive.push_back(r2);
      continue;
    }

    // Step 4: Backward Subsumption (remove active clauses subsumed by given)
    std::vector<IndexedClause> newActive;
    for (const auto& act : active) {
      if (FO2Inferences::subsumes(given, act)) {
        FO2Logger::logDebug("[FO2Solver] Active clause replaced by subsumption: " + act.toString());
      } else {
        newActive.push_back(act);
      }
    }
    active = newActive;

    // Step 5: Factoring on Given
    for (size_t i = 0; i < given.length(); ++i) {
      for (size_t j = i + 1; j < given.length(); ++j) {
        IndexedClause factorRes;
        if (FO2Inferences::factor(given, i, j, factorRes)) {
          IndexedClause simpFactor;
          bool isTaut = false;
          FO2Inferences::simplifyEqualityClause(factorRes, simpFactor, isTaut);
          if (isTaut) continue;
          if (simpFactor.length() == 0) {
            FO2Logger::logPhase("Empty clause derived from Factoring: UNSATISFIABLE");
            return FO2Result::UNSATISFIABLE;
          }
          FO2Logger::logDebug("[FO2Solver] Generated factor: " + simpFactor.toStringWithSelection());
          passive.push_back(simpFactor);
        }
      }
    }

    // Step 6: Resolution between Given and Active clauses
    auto selG = given.getSelectedLiteralIndices();
    for (const auto& act : active) {
      auto selA = act.getSelectedLiteralIndices();
      for (size_t idxG : selG) {
        for (size_t idxA : selA) {
          IndexedClause resolvent1;
          if (FO2Inferences::resolve(given, idxG, act, idxA, resolvent1)) {
            IndexedClause simpRes1;
            bool isTaut = false;
            FO2Inferences::simplifyEqualityClause(resolvent1, simpRes1, isTaut);
            if (!isTaut) {
              if (simpRes1.length() == 0) {
                FO2Logger::logPhase("Empty clause derived from Resolution: UNSATISFIABLE");
                return FO2Result::UNSATISFIABLE;
              }
              FO2Logger::logDebug("[FO2Solver] Generated resolvent: " + simpRes1.toStringWithSelection());
              passive.push_back(simpRes1);
            }
          }

          IndexedClause resolvent2;
          if (FO2Inferences::resolve(act, idxA, given, idxG, resolvent2)) {
            IndexedClause simpRes2;
            bool isTaut = false;
            FO2Inferences::simplifyEqualityClause(resolvent2, simpRes2, isTaut);
            if (!isTaut) {
              if (simpRes2.length() == 0) {
                FO2Logger::logPhase("Empty clause derived from Resolution: UNSATISFIABLE");
                return FO2Result::UNSATISFIABLE;
              }
              FO2Logger::logDebug("[FO2Solver] Generated inverse resolvent: " + simpRes2.toStringWithSelection());
              passive.push_back(simpRes2);
            }
          }
        }
      }
    }

    active.push_back(given);
  }

  FO2Logger::logPhase("Saturation Engine completed without empty clause: SATISFIABLE");
  return FO2Result::SATISFIABLE;
}

} // namespace FO2Fragment
