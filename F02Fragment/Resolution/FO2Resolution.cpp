#include "FO2Resolution.hpp"
#include "Kernel/TermIterators.hpp"
#include "Lib/DHSet.hpp"
#include <sstream>
#include <algorithm>

using namespace Kernel;
using namespace Lib;

namespace FO2Fragment {

bool hasFunctionalTerms(const Literal* lit)
{
  if (!lit) return false;
  unsigned arity = lit->arity();
  for (unsigned i = 0; i < arity; ++i) {
    const TermList* tl = lit->nthArgument(i);
    if (tl->isTerm() && tl->term()->arity() > 0) {
      return true;
    }
  }
  return false;
}

unsigned getTermDepth(const TermList tl)
{
  if (tl.isVar()) {
    return 0;
  }
  if (!tl.isTerm()) {
    return 0;
  }

  const Term* t = tl.term();
  unsigned maxChild = 0;
  unsigned arity = t->arity();
  for (unsigned i = 0; i < arity; ++i) {
    unsigned d = getTermDepth(*t->nthArgument(i));
    if (d > maxChild) {
      maxChild = d;
    }
  }
  return 1 + maxChild;
}

unsigned getLiteralDepth(const Literal* lit)
{
  if (!lit) return 0;
  unsigned maxD = 0;
  unsigned arity = lit->arity();
  for (unsigned i = 0; i < arity; ++i) {
    unsigned d = getTermDepth(*lit->nthArgument(i));
    if (d > maxD) {
      maxD = d;
    }
  }
  return maxD;
}

int compareGroundIndexedLiterals(const IndexedLiteral& ilitA, const IndexedLiteral& ilitB)
{
  unsigned depthA = getLiteralDepth(ilitA.literal);
  unsigned depthB = getLiteralDepth(ilitB.literal);

  if (depthA < depthB) return -1;
  if (depthA > depthB) return 1;

  return 0;
}

IndexedClause::IndexedClause() : _originClause(nullptr) {}

IndexedClause::IndexedClause(const std::vector<IndexedLiteral>& lits, Kernel::Clause* origin)
    : _literals(lits), _originClause(origin)
{
  DHSet<unsigned> clauseVars;
  for (const auto& ilit : _literals) {
    if (ilit.literal) {
      VariableIterator vit(ilit.literal);
      while (vit.hasNext()) {
        clauseVars.insert(vit.next().var());
      }
    }
  }

  const unsigned numClauseVars = clauseVars.size();
  for (auto& ilit : _literals) {
    if (!ilit.literal) continue;
    DHSet<unsigned> litVars;
    VariableIterator lvit(ilit.literal);
    while (lvit.hasNext()) {
      litVars.insert(lvit.next().var());
    }
    if (numClauseVars == 2 && litVars.size() == 2) {
      ilit.index = 1;
    } else {
      ilit.index = 0;
    }
  }
}

unsigned IndexedClause::varCount() const
{
  DHSet<unsigned> vars;
  for (const auto& ilit : _literals) {
    if (ilit.literal) {
      VariableIterator vit(ilit.literal);
      while (vit.hasNext()) {
        vars.insert(vit.next().var());
      }
    }
  }
  return vars.size();
}

bool IndexedClause::isGround() const
{
  return varCount() == 0;
}

std::string IndexedClause::toString() const
{
  if (_literals.empty()) {
    return "#indexed_empty#";
  }

  std::ostringstream ss;
  for (size_t i = 0; i < _literals.size(); ++i) {
    if (i > 0) {
      ss << " | ";
    }
    if (_literals[i].literal) {
      ss << _literals[i].literal->toString() << ":" << _literals[i].index;
    } else {
      ss << "null:" << _literals[i].index;
    }
  }
  return ss.str();
}

bool IndexedClause::isSelected(size_t litIndex) const
{
  if (litIndex >= _literals.size()) return false;
  
  // Lemma 9 says "pick a literal L". To maintain completeness (preventing Satisfiable when actually Unsatisfiable)
  // while avoiding exponential explosion, we must select EXACTLY ONE valid literal using a STRICT GLOBAL ORDERING.
  size_t best_idx = _literals.size();
  for (size_t i = 0; i < _literals.size(); ++i) {
    if (isValidSelectionCandidate(i)) {
      if (best_idx == _literals.size()) {
        best_idx = i;
      } else {
        const IndexedLiteral& A = _literals[i];
        const IndexedLiteral& B = _literals[best_idx];
        
        unsigned depthA = getLiteralDepth(A.literal);
        unsigned depthB = getLiteralDepth(B.literal);
        
        bool is_greater = false;
        if (depthA > depthB) {
            is_greater = true;
        } else if (depthA == depthB) {
            if (A.index > B.index) {
                is_greater = true;
            } else if (A.index == B.index) {
                if (A.literal->functor() > B.literal->functor()) {
                    is_greater = true;
                } else if (A.literal->functor() == B.literal->functor()) {
                    if (A.literal->polarity() > B.literal->polarity()) {
                        is_greater = true;
                    }
                }
            }
        }
        
        if (is_greater) {
            best_idx = i;
        }
      }
    }
  }
  return litIndex == best_idx;
}

bool IndexedClause::isValidSelectionCandidate(size_t litIndex) const
{
  if (litIndex >= _literals.size()) {
    return false;
  }

  bool clauseHasFunctional = false;
  bool clauseHasIndexOne = false;

  for (const auto& ilit : _literals) {
    if (hasFunctionalTerms(ilit.literal)) {
      clauseHasFunctional = true;
    }
    if (ilit.index == 1) {
      clauseHasIndexOne = true;
    }
  }

  const IndexedLiteral& target = _literals[litIndex];
  const bool targetHasFunc = hasFunctionalTerms(target.literal);

  // Lemma 9 requirement: Each literal selected by Σ2 contains all variables of its clause.
  DHSet<unsigned> clauseVars;
  for (const auto& ilit : _literals) {
    if (ilit.literal) {
      VariableIterator vit(ilit.literal);
      while (vit.hasNext()) {
        clauseVars.insert(vit.next().var());
      }
    }
  }
  
  DHSet<unsigned> targetVars;
  if (target.literal) {
    VariableIterator vit(target.literal);
    while (vit.hasNext()) {
      targetVars.insert(vit.next().var());
    }
  }
  
  DHSet<unsigned>::Iterator cit(clauseVars);
  while (cit.hasNext()) {
    if (!targetVars.contains(cit.next())) {
      return false; // Does not contain all variables of the clause
    }
  }

  // Condition 1: A has no functional terms, a = 0, and there is a literal B:b in c with b = 1.
  if (!targetHasFunc && target.index == 0 && clauseHasIndexOne) {
    return false;
  }

  // Condition 2: A:a has no functional terms, and there are literals with functional terms in c.
  if (!targetHasFunc && clauseHasFunctional) {
    return false;
  }

  // Fallback if no literal could be selected because of these rules?
  // Lemma 9 guarantees at least one literal is selectable for S2 clauses.
  return true;
}

std::vector<size_t> IndexedClause::getSelectedLiteralIndices() const
{
  std::vector<size_t> selected;
  for (size_t i = 0; i < _literals.size(); ++i) {
    if (isSelected(i)) {
      selected.push_back(i);
    }
  }
  return selected;
}

std::string IndexedClause::toStringWithSelection() const
{
  if (_literals.empty()) {
    return "#indexed_empty#";
  }

  std::ostringstream ss;
  for (size_t i = 0; i < _literals.size(); ++i) {
    if (i > 0) {
      ss << " | ";
    }
    bool sel = isSelected(i);
    ss << (sel ? "[SEL] " : "[   ] ");
    if (_literals[i].literal) {
      ss << _literals[i].literal->toString() << ":" << _literals[i].index;
    } else {
      ss << "null:" << _literals[i].index;
    }
  }
  return ss.str();
}

IndexedClause IndexedClause::fromClause(Clause* cl)
{
  if (!cl) {
    return IndexedClause();
  }

  DHSet<unsigned> clauseVars;
  VirtualIterator<unsigned> vit = cl->getVariableIterator();
  while (vit.hasNext()) {
    clauseVars.insert(vit.next());
  }

  const unsigned numClauseVars = clauseVars.size();
  const unsigned len = cl->length();
  std::vector<IndexedLiteral> indexedLits;
  indexedLits.reserve(len);

  for (unsigned i = 0; i < len; ++i) {
    Literal* lit = (*cl)[i];
    DHSet<unsigned> litVars;
    VariableIterator lvit(lit);
    while (lvit.hasNext()) {
      litVars.insert(lvit.next().var());
    }

    unsigned idx = 0;
    if (numClauseVars == 2 && litVars.size() == 2) {
      idx = 1;
    } else {
      idx = 0;
    }

    indexedLits.push_back(IndexedLiteral(lit, idx));
  }

  return IndexedClause(indexedLits, cl);
}

} // namespace FO2Fragment
