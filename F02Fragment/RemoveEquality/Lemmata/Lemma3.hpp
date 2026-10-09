#ifndef FO2_LEMMA3_HPP
#define FO2_LEMMA3_HPP

#include "Kernel/Problem.hpp"

namespace FO2Fragment {

/**
 * @brief Implements Lemma 3 from de Nivelle and Pratt-Hartmann: move equality
 *        out of the CNF matrices of Scott-normal-form formulas.
 */
class Lemma3 {
public:
  /**
   * @brief Applies Lemma 3 to the given problem.
   * @param prb Problem instance to transform.
   */
  static void applyLemma3(Kernel::Problem &prb);
};

} // namespace FO2Fragment

#endif // FO2_LEMMA3_HPP
