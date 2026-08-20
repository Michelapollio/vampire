#ifndef __FO2Solver__
#define __FO2Solver__

#include "Kernel/Problem.hpp"
#include "FO2Resolution.hpp"
#include "FO2Inferences.hpp"
#include <vector>
#include <deque>

namespace FO2Fragment {

enum class FO2Result {
  SATISFIABLE,
  UNSATISFIABLE,
  UNKNOWN
};

class FO2Solver {
public:
  /**
   * @brief Solves an FO2 problem (Section 4 decision procedure).
   * Returns SATISFIABLE or UNSATISFIABLE.
   */
  static FO2Result solve(Kernel::Problem& prb);
};

} // namespace FO2Fragment

#endif // __FO2Solver__
