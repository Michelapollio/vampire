#ifndef __FO2Resolution__
#define __FO2Resolution__

#include "Kernel/Clause.hpp"
#include "Kernel/Term.hpp"
#include "Lib/DHSet.hpp"
#include <vector>
#include <string>

namespace FO2Fragment {

/**
 * @brief Struct representing an indexed literal (L:a) according to Section 4.1 / Definition 3 of de Nivelle & Pratt-Hartmann.
 */
struct IndexedLiteral {
  Kernel::Literal* literal;
  unsigned index; // 0 or 1

  IndexedLiteral() : literal(nullptr), index(0) {}
  IndexedLiteral(Kernel::Literal* lit, unsigned idx) : literal(lit), index(idx) {}

  bool operator==(const IndexedLiteral& other) const {
    return literal == other.literal && index == other.index;
  }
};

/**
 * @brief Utility functions for terms, depth, and order on literals (Section 4.3).
 */
bool hasFunctionalTerms(const Kernel::Literal* lit);
unsigned getTermDepth(const Kernel::TermList tl);
unsigned getLiteralDepth(const Kernel::Literal* lit);

/**
 * @brief Order <2 on ground indexed literals (Definition 8).
 * - A:a <2 B:b if A is strictly less deep than B.
 * - A:a <2 B:b if A and B have equal depth and a < b.
 * Returns -1 if A:a <2 B:b, +1 if B:b <2 A:a, 0 if equal.
 */
int compareGroundIndexedLiterals(const IndexedLiteral& ilitA, const IndexedLiteral& ilitB);

/**
 * @brief Class representing an indexed clause (S2+i clause) according to Definition 7.
 */
class IndexedClause {
private:
  std::vector<IndexedLiteral> _literals;
  Kernel::Clause* _originClause;

public:
  IndexedClause();
  IndexedClause(const std::vector<IndexedLiteral>& lits, Kernel::Clause* origin = nullptr);

  size_t length() const { return _literals.size(); }
  const IndexedLiteral& operator[](size_t i) const { return _literals[i]; }
  IndexedLiteral& operator[](size_t i) { return _literals[i]; }

  const std::vector<IndexedLiteral>& literals() const { return _literals; }
  Kernel::Clause* originClause() const { return _originClause; }

  unsigned varCount() const;
  bool isGround() const;
  
  std::string toString() const;
  std::string toStringWithSelection() const;

  /**
   * @brief Implements Selection Function \Sigma_2 (Definition 9).
   * Returns true if literal at position litIndex is selected in this clause.
   */
  bool isSelected(size_t litIndex) const;

  /**
   * @brief Returns indices of all selected literals in this clause according to \Sigma_2.
   */
  std::vector<size_t> getSelectedLiteralIndices() const;

  /**
   * @brief Factory method: creates an IndexedClause from a standard Kernel::Clause according to S2+i indexing (Definition 7).
   * - If clause contains 2 variables, literals with 2 variables get index 1, others get index 0.
   * - If clause contains <= 1 variable, all literals get index 0.
   */
  static IndexedClause fromClause(Kernel::Clause* cl);
};

} // namespace FO2Fragment

#endif // __FO2Resolution__
