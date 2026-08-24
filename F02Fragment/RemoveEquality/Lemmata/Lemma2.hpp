#ifndef FO2_LEMMA2_HPP
#define FO2_LEMMA2_HPP

#include "Kernel/Problem.hpp"


namespace FO2Fragment {

/**
 * @brief Implementation of Lemma 2 (Introduction of definition predicates for complex subformulas).
 */
class Lemma2 {
public:
  /**
   * @brief Applies Lemma 2 to the given problem.
   * @param prb Problem instance to transform.
   */
  static void applyLemma2(Kernel::Problem &prb);
};
}

#endif