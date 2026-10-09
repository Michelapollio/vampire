#ifndef __FO2_LEMMA_4_HPP__
#define __FO2_LEMMA_4_HPP__

#include "Kernel/Problem.hpp"

namespace FO2Fragment {
namespace Lemmata {

/**
 * @brief Implements Lemma 4: restricted Type 3 saturation, cross-resolution
 *        into Type 2 clauses, and the equisatisfiable Type 3 reduction.
 */
class Lemma4 {
public:
  /**
   * @brief Applies Lemma 4 to the given problem.
   * @param prb Problem instance to transform.
  */
  static void applyLemma4(Kernel::Problem &prb);
};

} // namespace Lemmata
} // namespace FO2Fragment

#endif // __FO2_LEMMA_4_HPP__
