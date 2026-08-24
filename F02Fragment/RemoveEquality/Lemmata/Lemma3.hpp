#ifndef FO2_LEMMA3_HPP
#define FO2_LEMMA3_HPP

#include "Kernel/Problem.hpp"

namespace FO2Fragment {

/**
 * @brief Implementation of Lemma 3 (Conversion of Scott Type 2 formulas into Type 3 via Skolemization).
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
