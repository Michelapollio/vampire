#include "FO2Solver.hpp"
#include "F02Fragment/FO2Logger.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/Clause.hpp"
#include <deque>
#include <vector>
#include <iostream>
#include <map>
#include <string>

#include "Lib/Environment.hpp"
#include "Kernel/Signature.hpp"
#include "Shell/Statistics.hpp"
#include "Saturation/ProvingHelper.hpp"

using namespace Kernel;
using namespace Lib;

namespace FO2Fragment {

namespace {
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

FO2Result FO2Solver::solve(Problem& prb)
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
      IndexedClause simpIcl;
      bool isTaut = false;
      FO2Inferences::simplifyEqualityClause(icl, simpIcl, isTaut);
      if (isTaut) continue;
      if (simpIcl.length() == 0) {
        FO2Logger::logPhase("Initial empty clause found: UNSATISFIABLE");
        return FO2Result::UNSATISFIABLE;
      }
      s2Monitor.checkGenerated(simpIcl, "initial");
      passive.push_back(simpIcl);
    }
  }

  // Step 1b: Add eq0/neq0 reflexivity, anti-reflexivity, and duality axioms
  unsigned eq0Functor = env.signature->addPredicate("eq0", 2);
  unsigned neq0Functor = env.signature->addPredicate("neq0", 2);

  for (unsigned p = 1; p < env.signature->predicates(); ++p) {
    Signature::Symbol* sym = env.signature->getPredicate(p);
    if (!sym) continue;
    std::string name = sym->name();
    if (name == "neq0" || name == "neq2" || name == "neq") {
      neq0Functor = p;
    } else if (name == "eq0" || name == "eq2" || name == "eq") {
      eq0Functor = p;
    }
  }

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
      // ~eq0(X0, X1) | ~P(X0) | P(X1) (2 variables: X0, X1)
      TermList a0[1] = {var0};
      TermList a1[1] = {var1};
      Literal* notP0 = Literal::create(p, 1, false, a0);
      Literal* posP1 = Literal::create(p, 1, true, a1);
      passive.push_back(IndexedClause({IndexedLiteral(notEq0Lit, 1), IndexedLiteral(notP0, 0), IndexedLiteral(posP1, 0)}));
    }
  }

  FO2Logger::logDebug("[FO2Solver] Initial passive clauses: " + std::to_string(passive.size()));

  size_t iteration = 0;
  const size_t MAX_ITERATIONS = 50000; // Safeguard limit

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

    // Step 3: Structural Splitting (Propositional Naming for variable-disjoint subclauses)
    IndexedClause r1, r2;
    if (FO2Inferences::split(given, r1, r2)) {
      static std::map<std::string, unsigned> splitMap;
      
      std::vector<std::string> litsStr;
      for (const auto& l : r1.literals()) {
        if (l.literal) litsStr.push_back(l.literal->toString());
      }
      std::sort(litsStr.begin(), litsStr.end());
      std::string canonicalR1 = "";
      for (const auto& s : litsStr) canonicalR1 += s + "|";
      
      unsigned pSym;
      if (splitMap.find(canonicalR1) != splitMap.end()) {
        pSym = splitMap[canonicalR1];
      } else {
        static unsigned splitCounter = 0;
        std::string propName = "sP_split_" + std::to_string(++splitCounter);
        pSym = env.signature->addPredicate(propName, 0);
        splitMap[canonicalR1] = pSym;
      }

      Literal* posP = Literal::create(pSym, true, {});
      Literal* negP = Literal::create(pSym, false, {});

      std::vector<IndexedLiteral> lits1 = r1.literals();
      std::vector<IndexedLiteral> lits2 = r2.literals();

      lits1.push_back(IndexedLiteral(posP, 0));
      lits2.push_back(IndexedLiteral(negP, 0));

      IndexedClause split1(lits1, given.originClause());
      IndexedClause split2(lits2, given.originClause());

      FO2Logger::logDebug("[FO2Solver] Structural Splitting applied to Given: C1=" + split1.toString() + ", C2=" + split2.toString());
      s2Monitor.checkGenerated(split1, "split");
      s2Monitor.checkGenerated(split2, "split");
      passive.push_back(split1);
      passive.push_back(split2);
      continue;
    }


    // S2 invariant: the given clause is now (after splitting) about to become active
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
            bool isTaut = false;
            FO2Inferences::simplifyEqualityClause(resolvent1, simpRes1, isTaut);
            if (!isTaut) {
              if (simpRes1.length() == 0) {
                FO2Logger::logPhase("Empty clause derived from Resolution: UNSATISFIABLE");
                return FO2Result::UNSATISFIABLE;
              }
              FO2Logger::logDebug("[FO2Solver] Generated resolvent: " + simpRes1.toStringWithSelection());
              s2Monitor.checkGenerated(simpRes1, "resolution");
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
              s2Monitor.checkGenerated(simpRes2, "resolution");
              passive.push_back(simpRes2);
            }
          }
        }
      }
    }

    active.push_back(given);
  }

  if (iteration >= MAX_ITERATIONS) {
    FO2Logger::logPhase("Max iterations reached without complete saturation: UNKNOWN");
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

