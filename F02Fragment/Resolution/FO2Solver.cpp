#include "FO2Solver.hpp"
#include "F02Fragment/FO2Logger.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/Clause.hpp"
#include <deque>
#include <vector>
#include <iostream>

using namespace Kernel;

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
      if (icl.length() == 0) {
        FO2Logger::logPhase("Initial empty clause found: UNSATISFIABLE");
        return FO2Result::UNSATISFIABLE;
      }
      passive.push_back(icl);
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

    // Step 3: Splitting Rule check
    IndexedClause r1, r2;
    if (FO2Inferences::split(given, r1, r2)) {
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
          if (factorRes.length() == 0) {
            FO2Logger::logPhase("Empty clause derived from Factoring: UNSATISFIABLE");
            return FO2Result::UNSATISFIABLE;
          }
          FO2Logger::logDebug("[FO2Solver] Generated factor: " + factorRes.toStringWithSelection());
          passive.push_back(factorRes);
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
            if (resolvent1.length() == 0) {
              FO2Logger::logPhase("Empty clause derived from Resolution: UNSATISFIABLE");
              return FO2Result::UNSATISFIABLE;
            }
            FO2Logger::logDebug("[FO2Solver] Generated resolvent: " + resolvent1.toStringWithSelection());
            passive.push_back(resolvent1);
          }

          IndexedClause resolvent2;
          if (FO2Inferences::resolve(act, idxA, given, idxG, resolvent2)) {
            if (resolvent2.length() == 0) {
              FO2Logger::logPhase("Empty clause derived from Resolution: UNSATISFIABLE");
              return FO2Result::UNSATISFIABLE;
            }
            FO2Logger::logDebug("[FO2Solver] Generated inverse resolvent: " + resolvent2.toStringWithSelection());
            passive.push_back(resolvent2);
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
