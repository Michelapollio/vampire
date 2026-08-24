#ifndef __FO2_LEMMA_4_HPP__
#define __FO2_LEMMA_4_HPP__

#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"

namespace FO2Fragment {
namespace Lemmata {

/**
 * @brief Implementation of Lemma 4 (Restricted resolution on Type 3 clauses).
 */
class Lemma4 {
public:
  /**
   * @brief Applies Lemma 4 to the given problem.
   * @param prb Problem instance to transform.
   */
  static void applyLemma4(Kernel::Problem &prb);
  
private:
  static bool isBinaryLiteral(Kernel::Literal* lit);
  static bool areResolvable(Kernel::Literal* litA, Kernel::Literal* litB, bool& needsSwap);
  static Kernel::Literal* swapVarsInLiteral(Kernel::Literal* lit);
  static void resolveRestricted(Kernel::Clause* clA, Kernel::Clause* clB, std::vector<Kernel::Clause*>& newResolvents);

  // Extraction of Clauses from Type 3 FormulaUnits
  static Kernel::Literal* formulaToLiteral(Kernel::Formula* formula);
  static Kernel::Formula* stripForallXY(Kernel::Formula* formula);
  static void extractClausesFromBody(Kernel::Formula* body, Kernel::Unit* parent, std::vector<Kernel::Clause*>& out);
  static void extractType3Clauses(Kernel::FormulaUnit* fu, std::vector<Kernel::Clause*>& out);
};

} // namespace Lemmata
} // namespace FO2Fragment

#endif // __FO2_LEMMA_4_HPP__
