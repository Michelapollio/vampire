#include "FO2Solver.hpp"
#include "F02Fragment/FO2Logger.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/TermIterators.hpp"
#include "Lib/Stack.hpp"
#include <deque>
#include <vector>
#include <iostream>
#include <map>
#include <string>

#include "Lib/Environment.hpp"
#include "Shell/Statistics.hpp"
#include "Saturation/ProvingHelper.hpp"

using namespace Kernel;
using namespace Lib;

namespace FO2Fragment {

namespace {
/** Find an article-style split c = R1 v R2 with disjoint variable sets. */
bool partitionClause(Clause* clause, std::vector<Literal*>& r1, std::vector<Literal*>& r2)
{
  const size_t length = clause->length();
  if (length < 2) return false;

  std::vector<DHSet<unsigned>> vars(length);
  std::vector<size_t> variableLiterals;
  std::vector<size_t> groundLiterals;
  for (size_t i = 0; i < length; ++i) {
    VariableIterator it((*clause)[i]);
    while (it.hasNext()) vars[i].insert(it.next().var());
    if (vars[i].isEmpty()) groundLiterals.push_back(i);
    else variableLiterals.push_back(i);
  }

  // A ground part and a variable-bearing part are disjoint by definition.
  if (!groundLiterals.empty() && !variableLiterals.empty()) {
    for (size_t i : groundLiterals) r1.push_back((*clause)[i]);
    for (size_t i : variableLiterals) r2.push_back((*clause)[i]);
    return true;
  }

  std::vector<int> component(length, -1);
  int componentCount = 0;
  for (size_t i : variableLiterals) {
    if (component[i] != -1) continue;
    component[i] = componentCount;
    std::vector<size_t> queue{i};
    for (size_t q = 0; q < queue.size(); ++q) {
      const size_t current = queue[q];
      for (size_t j : variableLiterals) {
        if (component[j] != -1) continue;
        bool sharesVariable = false;
        DHSet<unsigned>::Iterator vit(vars[current]);
        while (vit.hasNext()) {
          if (vars[j].contains(vit.next())) { sharesVariable = true; break; }
        }
        if (sharesVariable) {
          component[j] = componentCount;
          queue.push_back(j);
        }
      }
    }
    ++componentCount;
  }

  if (componentCount < 2) return false;
  // Put the first connected component (and any ground literals) in R1;
  // collect all remaining components in R2. Their variable sets are disjoint.
  for (size_t i : groundLiterals) r1.push_back((*clause)[i]);
  for (size_t i : variableLiterals) {
    (component[i] == 0 ? r1 : r2).push_back((*clause)[i]);
  }
  return !r1.empty() && !r2.empty();
}

Problem makeSplitBranch(Problem& source, Clause* splitClause, const std::vector<Literal*>& part)
{
  Stack<Literal*> literals;
  for (Literal* literal : part) literals.push(literal);
  Clause* branchClause = Clause::fromStack(literals, splitClause->inference());

  UnitList* units = UnitList::empty();
  UnitList::Iterator it(source.units());
  while (it.hasNext()) {
    Unit* unit = it.next();
    if (unit != splitClause) UnitList::push(unit, units);
  }
  UnitList::push(branchClause, units);
  return Problem(UnitList::reverse(units));
}

/**
 * Monitors the invariance of the class S2 (Lemma 10): every clause that enters the
 * saturation (initial clauses and clauses produced by resolution, factoring and splitting)
 * is checked against Definition 6. Prints a summary when the solver terminates.
 */
struct S2Monitor {
  struct Stats {
    size_t checked = 0;
    size_t violations = 0;
    std::map<std::string, size_t> byOrigin;
    std::map<std::string, size_t> byReason;
    bool firstReported = false;
  };
  Stats generated; // raw output of the inferences (before splitting)
  Stats active;    // clauses entering the active set (after splitting)
  Stats indexGen;  // index condition of Lemma 10 on generated clauses
  Stats indexAct;  // index condition of Lemma 10 on active clauses
  Stats selection; // Lemma 9: selected literals contain all variables (active clauses)

  static void recordWith(bool ok, const std::string& reason, Stats& st, const char* stage, const IndexedClause& icl, const std::string& origin)
  {
    ++st.checked;
    if (ok) return;
    ++st.violations;
    ++st.byOrigin[origin];
    ++st.byReason[reason];
    if (!st.firstReported) {
      st.firstReported = true;
      FO2Logger::logPhase(std::string("[S2 INVARIANT] FIRST VIOLATION (") + stage + ", origin: " + origin + ", " + reason + "): " + icl.toStringWithSelection());
    }
  }

  static void record(Stats& st, const char* stage, const IndexedClause& icl, const std::string& origin)
  {
    std::string reason;
    bool ok = FO2Inferences::checkS2Invariant(icl, reason);
    recordWith(ok, reason, st, stage, icl, origin);
  }

  static void recordIndex(Stats& st, const char* stage, const IndexedClause& icl, const std::string& origin)
  {
    std::string reason;
    bool ok = FO2Inferences::checkIndexInvariant(icl, reason);
    recordWith(ok, reason, st, stage, icl, origin);
  }

  void checkGenerated(const IndexedClause& icl, const std::string& origin)
  {
    record(generated, "generated", icl, origin);
    recordIndex(indexGen, "generated/index", icl, origin);
  }
  void checkActive(const IndexedClause& icl)
  {
    record(active, "active", icl, "given");
    recordIndex(indexAct, "active/index", icl, "given");
    std::string reason;
    bool ok = FO2Inferences::checkSelectionCoversVars(icl, reason);
    recordWith(ok, reason, selection, "active/selection", icl, "given");
  }

  static void report(const char* stage, const Stats& st)
  {
    FO2Logger::logPhase(std::string("[S2 INVARIANT] ") + stage + ": checked " + std::to_string(st.checked) + " clauses, violations: " + std::to_string(st.violations));
    for (const auto& kv : st.byOrigin) {
      FO2Logger::logPhase(std::string("[S2 INVARIANT]   ") + stage + " by origin  " + kv.first + ": " + std::to_string(kv.second));
    }
    for (const auto& kv : st.byReason) {
      FO2Logger::logPhase(std::string("[S2 INVARIANT]   ") + stage + " by reason  " + kv.first + ": " + std::to_string(kv.second));
    }
  }

  ~S2Monitor()
  {
    report("generated (before splitting)", generated);
    report("active (after splitting)", active);
    report("index condition, generated", indexGen);
    report("index condition, active", indexAct);
    report("Lemma 9 selection, active", selection);
  }
};
} // namespace

// Shared by splitting and saturation, so the bound applies to the complete
// search rather than being restarted for every branch.
static constexpr size_t MAX_TOTAL_ITERATIONS = 50000;

static FO2Result solveSaturated(Problem& prb, size_t& totalIterations);

FO2Result FO2Solver::solve(Problem& prb)
{
  // Splitting creates alternative problems. Explore them iteratively so a
  // long chain of valid article-style splits cannot exhaust the C++ call stack.
  std::vector<UnitList*> pending{prb.units()};
  bool sawUnknown = false;
  size_t totalIterations = 0;
  while (!pending.empty()) {
    if (totalIterations >= MAX_TOTAL_ITERATIONS) {
      FO2Logger::logPhase("FO2 total iteration limit reached while exploring split branches: UNKNOWN");
      sawUnknown = true;
      break;
    }
    ++totalIterations; // Count each branch selected for examination.

    UnitList* units = pending.back();
    pending.pop_back();
    Problem current(units);

    bool hasEquality = false;
    UnitList::Iterator equalityCheck(current.units());
    while (equalityCheck.hasNext() && !hasEquality) {
      Unit* unit = equalityCheck.next();
      if (!unit->isClause()) continue;
      Clause* clause = static_cast<Clause*>(unit);
      for (unsigned i = 0; i < clause->length(); ++i) {
        if ((*clause)[i]->isEquality()) {
          hasEquality = true;
          break;
        }
      }
    }
    if (hasEquality) {
      FO2Logger::logPhase("Input still contains built-in equality; expected equality-free clauses: UNKNOWN");
      sawUnknown = true;
      continue;
    }

    bool split = false;
    UnitList::Iterator splitIt(current.units());
    while (splitIt.hasNext()) {
      Unit* unit = splitIt.next();
      if (!unit->isClause()) continue;
      Clause* clause = static_cast<Clause*>(unit);
      std::vector<Literal*> r1, r2;
      if (!partitionClause(clause, r1, r2)) continue;

      Problem branch1 = makeSplitBranch(current, clause, r1);
      Problem branch2 = makeSplitBranch(current, clause, r2);
      // LIFO: visit R1 first, while preserving R2 as the alternative branch.
      pending.push_back(branch2.units());
      pending.push_back(branch1.units());
      split = true;
      break;
    }
    if (split) continue;

    FO2Result result = solveSaturated(current, totalIterations);
    if (result == FO2Result::SATISFIABLE) return result;
    if (result == FO2Result::UNKNOWN) sawUnknown = true;
  }

  return sawUnknown ? FO2Result::UNKNOWN : FO2Result::UNSATISFIABLE;
}

static FO2Result solveSaturated(Problem& prb, size_t& totalIterations)
{
  FO2Logger::logPhase("Starting FO2 Saturation Engine (Section 4)");

  S2Monitor s2Monitor;
  std::deque<IndexedClause> passive;
  std::vector<IndexedClause> active;

  // Step 1: Collect and index initial clauses (S2+i class)
  UnitList::Iterator uit(prb.units());
  while (uit.hasNext()) {
    Unit* u = uit.next();
    if (u->isClause()) {
      Clause* cl = static_cast<Clause*>(u);
      IndexedClause icl = IndexedClause::fromClause(cl);
      for (const auto& ilit : icl.literals()) {
        if (!ilit.literal) continue;
        for (unsigned arg = 0; arg < ilit.literal->arity(); ++arg) {
          const TermList* termList = ilit.literal->nthArgument(arg);
          if (termList->isTerm() && termList->term()->arity() > 0 && termList->term()->arity() != 1) {
            FO2Logger::logPhase("Input is outside S2+i (non-unary function symbol): UNKNOWN");
            return FO2Result::UNKNOWN;
          }
        }
      }
      std::string invariantReason;
      if (!FO2Inferences::checkS2Invariant(icl, invariantReason) ||
          !FO2Inferences::checkIndexInvariant(icl, invariantReason)) {
        FO2Logger::logPhase("Input is outside S2+i (" + invariantReason + "): UNKNOWN");
        return FO2Result::UNKNOWN;
      }
      IndexedClause simpIcl;
      FO2Inferences::normalizeVariables(icl, simpIcl);
      if (simpIcl.length() == 0) {
        FO2Logger::logPhase("Initial empty clause found: UNSATISFIABLE");
        return FO2Result::UNSATISFIABLE;
      }
      s2Monitor.checkGenerated(simpIcl, "initial");
      passive.push_back(simpIcl);
    }
  }

  FO2Logger::logDebug("[FO2Solver] Initial passive clauses: " + std::to_string(passive.size()));

  while (!passive.empty() && totalIterations < MAX_TOTAL_ITERATIONS) {
    ++totalIterations; // Count each given-clause processing step.

    IndexedClause given = passive.front();
    passive.pop_front();

    if (given.length() == 0) {
      FO2Logger::logPhase("Derived empty clause found: UNSATISFIABLE");
      return FO2Result::UNSATISFIABLE;
    }

    FO2Logger::logDebug("[FO2Solver Step " + std::to_string(totalIterations) + "] Given: " + given.toStringWithSelection());

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

    // Clauses must enter in S2+i after the branching split rule above.
    s2Monitor.checkActive(given);

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
    const auto selectedForFactoring = given.getSelectedLiteralIndices();
    for (size_t i : selectedForFactoring) {
      for (size_t j = 0; j < given.length(); ++j) {
        if (i == j) continue;
        IndexedClause factorRes;
        if (FO2Inferences::factor(given, i, j, factorRes)) {
          IndexedClause simpFactor;
          FO2Inferences::normalizeVariables(factorRes, simpFactor);
          if (simpFactor.length() == 0) {
            FO2Logger::logPhase("Empty clause derived from Factoring: UNSATISFIABLE");
            return FO2Result::UNSATISFIABLE;
          }
          FO2Logger::logDebug("[FO2Solver] Generated factor: " + simpFactor.toStringWithSelection());
          s2Monitor.checkGenerated(simpFactor, "factoring");
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
            FO2Inferences::normalizeVariables(resolvent1, simpRes1);
            if (simpRes1.length() == 0) {
              FO2Logger::logPhase("Empty clause derived from Resolution: UNSATISFIABLE");
              return FO2Result::UNSATISFIABLE;
            }
            FO2Logger::logDebug("[FO2Solver] Generated resolvent: " + simpRes1.toStringWithSelection());
            s2Monitor.checkGenerated(simpRes1, "resolution");
            passive.push_back(simpRes1);
          }

          IndexedClause resolvent2;
          if (FO2Inferences::resolve(act, idxA, given, idxG, resolvent2)) {
            IndexedClause simpRes2;
            FO2Inferences::normalizeVariables(resolvent2, simpRes2);
            if (simpRes2.length() == 0) {
              FO2Logger::logPhase("Empty clause derived from Resolution: UNSATISFIABLE");
              return FO2Result::UNSATISFIABLE;
            }
            FO2Logger::logDebug("[FO2Solver] Generated inverse resolvent: " + simpRes2.toStringWithSelection());
            s2Monitor.checkGenerated(simpRes2, "resolution");
            passive.push_back(simpRes2);
          }
        }
      }
    }

    active.push_back(given);
  }

  if (!passive.empty()) {
    FO2Logger::logPhase("FO2 total iteration limit reached without complete saturation: UNKNOWN");
    return FO2Result::UNKNOWN;
  }

  FO2Logger::logPhase("Saturation Engine completed without empty clause: verifying ground saturation");

  UnitList* copyUnits = UnitList::empty();
  UnitList::Iterator uit2(prb.units());
  while (uit2.hasNext()) {
    UnitList::push(uit2.next(), copyUnits);
  }
  Problem copyPrb(UnitList::reverse(copyUnits));
  Saturation::ProvingHelper::runVampireSaturation(copyPrb, *env.options);
  if (env.statistics->terminationReason == Shell::TerminationReason::REFUTATION) {
    FO2Logger::logPhase("Ground contradiction verified by saturation engine: UNSATISFIABLE");
    return FO2Result::UNSATISFIABLE;
  }

  FO2Logger::logPhase("Saturation Engine completed without empty clause: SATISFIABLE");
  return FO2Result::SATISFIABLE;
}

} // namespace FO2Fragment
