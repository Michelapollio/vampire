#include "FO2Inferences.hpp"
#include "Kernel/RobSubstitution.hpp"
#include "Kernel/TermIterators.hpp"
#include "Lib/DHSet.hpp"

using namespace Kernel;
using namespace Lib;

namespace FO2Fragment {

bool FO2Inferences::resolve(const IndexedClause& icl1, size_t litIdx1,
                            const IndexedClause& icl2, size_t litIdx2,
                            IndexedClause& outResolvent)
{
  if (litIdx1 >= icl1.length() || litIdx2 >= icl2.length()) return false;
  if (!icl1.isSelected(litIdx1) || !icl2.isSelected(litIdx2)) return false;

  Literal* lit1 = icl1[litIdx1].literal;
  Literal* lit2 = icl2[litIdx2].literal;
  if (!lit1 || !lit2) return false;

  // Resolution requires complementary polarity
  if (lit1->polarity() == lit2->polarity()) return false;

  Literal* posLit1 = Literal::create(lit1, true);
  Literal* posLit2 = Literal::create(lit2, true);

  RobSubstitution subst;
  if (!subst.unify(TermList(posLit1), 0, TermList(posLit2), 1)) {
    return false;
  }

  std::vector<IndexedLiteral> resLits;

  // Add remaining literals from icl1 (bank 0)
  for (size_t i = 0; i < icl1.length(); ++i) {
    if (i == litIdx1) continue;
    Literal* subbedLit = subst.apply(icl1[i].literal, 0);
    IndexedLiteral ilit(subbedLit, icl1[i].index);
    bool dup = false;
    for (const auto& existing : resLits) {
      if (existing == ilit) { dup = true; break; }
    }
    if (!dup) resLits.push_back(ilit);
  }

  // Add remaining literals from icl2 (bank 1)
  for (size_t i = 0; i < icl2.length(); ++i) {
    if (i == litIdx2) continue;
    Literal* subbedLit = subst.apply(icl2[i].literal, 1);
    IndexedLiteral ilit(subbedLit, icl2[i].index);
    bool dup = false;
    for (const auto& existing : resLits) {
      if (existing == ilit) { dup = true; break; }
    }
    if (!dup) resLits.push_back(ilit);
  }

  // Tautology check: if resolvent contains complementary literals with same index
  for (size_t i = 0; i < resLits.size(); ++i) {
    for (size_t j = i + 1; j < resLits.size(); ++j) {
      if (Literal::complementaryLiteral(resLits[i].literal) == resLits[j].literal) {
        return false;
      }
    }
  }

  outResolvent = IndexedClause(resLits);
  return true;
}

bool FO2Inferences::factor(const IndexedClause& icl, size_t litIdx1, size_t litIdx2,
                           IndexedClause& outFactor)
{
  if (litIdx1 >= icl.length() || litIdx2 >= icl.length() || litIdx1 == litIdx2) return false;
  if (!icl.isSelected(litIdx1)) return false;

  Literal* lit1 = icl[litIdx1].literal;
  Literal* lit2 = icl[litIdx2].literal;
  if (!lit1 || !lit2) return false;

  // Factoring requires same polarity and same index
  if (icl[litIdx1].index != icl[litIdx2].index) return false;
  if (lit1->polarity() != lit2->polarity()) return false;

  Literal* posLit1 = Literal::create(lit1, true);
  Literal* posLit2 = Literal::create(lit2, true);

  RobSubstitution subst;
  if (!subst.unify(TermList(posLit1), 0, TermList(posLit2), 0)) {
    return false;
  }

  std::vector<IndexedLiteral> factorLits;
  for (size_t i = 0; i < icl.length(); ++i) {
    if (i == litIdx2) continue;
    Literal* subbedLit = subst.apply(icl[i].literal, 0);
    IndexedLiteral ilit(subbedLit, icl[i].index);
    bool dup = false;
    for (const auto& existing : factorLits) {
      if (existing == ilit) { dup = true; break; }
    }
    if (!dup) factorLits.push_back(ilit);
  }

  outFactor = IndexedClause(factorLits);
  return true;
}

namespace {
static bool matchSubsumptionLiterals(size_t litIdx1, const IndexedClause& icl1, const IndexedClause& icl2, RobSubstitution& subst) {
  if (litIdx1 >= icl1.length()) {
    return true;
  }

  const IndexedLiteral& ilit1 = icl1[litIdx1];
  for (size_t j = 0; j < icl2.length(); ++j) {
    const IndexedLiteral& ilit2 = icl2[j];
    if (ilit1.index != ilit2.index) continue;
    if (ilit1.literal->polarity() != ilit2.literal->polarity()) continue;
    if (ilit1.literal->functor() != ilit2.literal->functor()) continue;

    BacktrackData bd;
    subst.bdRecord(bd);

    Literal* pos1 = Literal::create(ilit1.literal, true);
    Literal* pos2 = Literal::create(ilit2.literal, true);
    if (subst.match(TermList(pos1), 0, TermList(pos2), 1)) {
      if (matchSubsumptionLiterals(litIdx1 + 1, icl1, icl2, subst)) {
        subst.bdDone();
        bd.drop();
        return true;
      }
    }
    subst.bdDone();
    bd.backtrack();
  }
  return false;
}
} // namespace

bool FO2Inferences::subsumes(const IndexedClause& icl1, const IndexedClause& icl2)
{
  if (icl1.length() > icl2.length()) return false;
  if (icl1.length() == 0) return true;

  RobSubstitution subst;
  return matchSubsumptionLiterals(0, icl1, icl2, subst);
}

bool FO2Inferences::split(const IndexedClause& icl, IndexedClause& outR1, IndexedClause& outR2)
{
  const size_t len = icl.length();
  if (len <= 1) return false;

  std::vector<DHSet<unsigned>> litVars(len);
  for (size_t i = 0; i < len; ++i) {
    if (icl[i].literal) {
      VariableIterator vit(icl[i].literal);
      while (vit.hasNext()) {
        litVars[i].insert(vit.next().var());
      }
    }
  }

  std::vector<int> component(len, -1);
  int compCount = 0;

  for (size_t i = 0; i < len; ++i) {
    if (component[i] != -1) continue;
    component[i] = compCount;

    std::vector<size_t> queue;
    queue.push_back(i);
    size_t qHead = 0;

    while (qHead < queue.size()) {
      size_t curr = queue[qHead++];
      for (size_t j = 0; j < len; ++j) {
        if (component[j] != -1) continue;
        bool share = false;
        DHSet<unsigned>::Iterator it(litVars[curr]);
        while (it.hasNext()) {
          if (litVars[j].contains(it.next())) {
            share = true;
            break;
          }
        }
        if (share) {
          component[j] = compCount;
          queue.push_back(j);
        }
      }
    }
    compCount++;
  }

  if (compCount <= 1) {
    return false;
  }

  std::vector<IndexedLiteral> r1Lits;
  std::vector<IndexedLiteral> r2Lits;

  for (size_t i = 0; i < len; ++i) {
    if (component[i] == 0) {
      r1Lits.push_back(icl[i]);
    } else {
      r2Lits.push_back(icl[i]);
    }
  }

  outR1 = IndexedClause(r1Lits);
  outR2 = IndexedClause(r2Lits);
  return true;
}

bool FO2Inferences::simplifyEqualityClause(const IndexedClause& inIcl, IndexedClause& outIcl, bool& outIsTautology)
{
  outIsTautology = false;
  std::vector<IndexedLiteral> currentLits = inIcl.literals();
  bool modified = true;

  while (modified) {
    modified = false;

    // 1. Check for tautology x = x (positive equality reflexivity)
    for (size_t i = 0; i < currentLits.size(); ++i) {
      Literal* lit = currentLits[i].literal;
      if (lit && lit->isEquality()) {
        TermList t1 = *lit->nthArgument(0);
        TermList t2 = *lit->nthArgument(1);
        if (lit->isPositive()) {
          if (t1.sameContent(&t2)) {
            outIsTautology = true;
            outIcl = IndexedClause();
            return true;
          }
        }
      }
    }

    // 2. Perform Equality Resolution on negative equality literals (t1 != t2)
    for (size_t i = 0; i < currentLits.size(); ++i) {
      Literal* lit = currentLits[i].literal;
      if (lit && lit->isEquality() && !lit->isPositive()) {
        TermList t1 = *lit->nthArgument(0);
        TermList t2 = *lit->nthArgument(1);

        RobSubstitution subst;
        if (subst.unify(t1, 0, t2, 0)) {
          std::vector<IndexedLiteral> newLits;
          for (size_t j = 0; j < currentLits.size(); ++j) {
            if (i == j) continue;
            Literal* origLit = currentLits[j].literal;
            Literal* newLit = subst.apply(origLit, 0);
            newLits.push_back(IndexedLiteral(newLit, currentLits[j].index));
          }
          currentLits = newLits;
          modified = true;
          break;
        }
      }
    }
  }

  // 3. Convert any remaining equality literals to eq0/neq0 predicates
  static unsigned eq0Functor = env.signature->addPredicate("eq0", 2);
  static unsigned neq0Functor = env.signature->addPredicate("neq0", 2);

  for (size_t i = 0; i < currentLits.size(); ++i) {
    Literal* lit = currentLits[i].literal;
    if (lit && lit->isEquality()) {
      TermList args[2] = {*lit->nthArgument(0), *lit->nthArgument(1)};
      Literal* proxyLit = nullptr;
      if (lit->isPositive()) {
        proxyLit = Literal::create(eq0Functor, 2, true, args);
      } else {
        proxyLit = Literal::create(neq0Functor, 2, true, args);
      }
      currentLits[i] = IndexedLiteral(proxyLit, currentLits[i].index);
    }
  }

  outIcl = IndexedClause(currentLits);
  return true;
}

} // namespace FO2Fragment
