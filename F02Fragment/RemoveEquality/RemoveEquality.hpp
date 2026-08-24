#ifndef __FO2_REMOVE_EQUALITY__
#define __FO2_REMOVE_EQUALITY__

#include "Kernel/Formula.hpp"
#include "Kernel/Problem.hpp"

namespace Kernel {
  class Clause;
}

namespace FO2Fragment {

/**
 * @brief Handles elimination of equality literals in FO2 problems.
 * Applies Lemmata 1 through 6 sequentially to eliminate equality from FO2 formulas
 * while preserving satisfiability.
 */
class RemoveEquality {
public:
  /**
   * @brief Removes equality from an FO2 problem by applying Lemmata 1 to 6.
   * @param prb Problem instance from which equality will be eliminated.
   */
  static void removeEquality(Kernel::Problem& prb);
  

  // Tracing diagnostic functions (read-only, do not modify the problem)
  static void traceProblem(Kernel::Problem& prb);
  static void traceFormula(Kernel::Formula* formula, int depth);
  static void traceClause(Kernel::Clause* cl);

private:
  static void Lemma1Application(Kernel::Problem& prb);
  static void Lemma2Application(Kernel::Problem& prb);
  static void Lemma3Application(Kernel::Problem& prb);
  static void Lemma4Application(Kernel::Problem& prb);
  static void Lemma5Application(Kernel::Problem& prb);
  static void Lemma6Application(Kernel::Problem& prb);


};

} // namespace FO2Fragment

#endif
