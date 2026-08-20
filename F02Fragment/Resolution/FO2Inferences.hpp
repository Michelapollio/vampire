#ifndef __FO2Inferences__
#define __FO2Inferences__

#include "FO2Resolution.hpp"
#include <vector>

namespace FO2Fragment {

class FO2Inferences {
public:
  /**
   * @brief Performs resolution between selected literals of icl1 and icl2.
   * Resolves selected literal at position litIdx1 in icl1 with selected literal at position litIdx2 in icl2.
   * Resolvent literals inherit indices from parent literals.
   * Returns true if resolution produced a resolvent, false otherwise.
   */
  static bool resolve(const IndexedClause& icl1, size_t litIdx1,
                      const IndexedClause& icl2, size_t litIdx2,
                      IndexedClause& outResolvent);

  /**
   * @brief Performs factoring on an indexed clause.
   * Factors selected literal at position litIdx1 with literal at position litIdx2.
   * Returns true if factoring produced a factor, false otherwise.
   */
  static bool factor(const IndexedClause& icl, size_t litIdx1, size_t litIdx2,
                     IndexedClause& outFactor);

  /**
   * @brief Checks indexed subsumption (Definition 4.1).
   * Returns true if icl1 subsumes icl2 (matching literals AND indices).
   */
  static bool subsumes(const IndexedClause& icl1, const IndexedClause& icl2);

  /**
   * @brief Checks if an indexed clause can be split into variable-disjoint subclauses (Definition 4.1 Splitting).
   * Returns true if split succeeded, populating outR1 and outR2.
   */
  static bool split(const IndexedClause& icl, IndexedClause& outR1, IndexedClause& outR2);
};

} // namespace FO2Fragment

#endif // __FO2Inferences__
