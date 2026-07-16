#ifndef __FO2_REMOVE_EQUALITY__
#define __FO2_REMOVE_EQUALITY__

#include "Kernel/Formula.hpp"
#include "Kernel/Problem.hpp"

namespace Kernel {
  class Clause;
}

namespace FO2Fragment {

class RemoveEquality {
public:
  static void removeEquality(Kernel::Problem& prb);
  static void applyLemma1(Kernel::Problem& prb);

  // Funzioni diagnostiche di tracing (sola lettura, non modificano il problema)
  static void traceProblem(Kernel::Problem& prb);
  static void traceFormula(Kernel::Formula* formula, int depth);
  static void traceClause(Kernel::Clause* cl);

private:
  
  static void applyLemma2(Kernel::Problem& prb);
  static void applyLemma3(Kernel::Problem& prb);
  static void applyLemma4(Kernel::Problem& prb);
  static void applyLemma5(Kernel::Problem& prb);
  static void applyLemma6(Kernel::Problem& prb);


};

} // namespace FO2Fragment

#endif
