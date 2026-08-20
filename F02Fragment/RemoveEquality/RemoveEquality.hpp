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
  

  // Funzioni diagnostiche di tracing (sola lettura, non modificano il problema)
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
