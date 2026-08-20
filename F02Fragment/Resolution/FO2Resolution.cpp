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

  if (ilitA.index < ilitB.index) return -1;
  if (ilitA.index > ilitB.index) return 1;

  return 0;
}

IndexedClause::IndexedClause() : _originClause(nullptr) {}

IndexedClause::IndexedClause(const std::vector<IndexedLiteral>& lits, Kernel::Clause* origin)
    : _literals(lits), _originClause(origin) {}

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

  // Condition 1: A has no functional terms, a = 0, and there is a literal B:b in c with b = 1.
  if (!targetHasFunc && target.index == 0 && clauseHasIndexOne) {
    return false;
  }

  // Condition 2: A:a has no functional terms, and there are literals with functional terms in c.
  if (!targetHasFunc && clauseHasFunctional) {
    return false;
  }

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
