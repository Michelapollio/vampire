#ifndef FO2_LEMMA1_HPP
#define FO2_LEMMA1_HPP

#include "Kernel/Problem.hpp"

namespace FO2Fragment{

/**
 * @brief Implementation of Lemma 1 (Replacement of equality literals with constant substitutions).
 */
class Lemma1 {
    public:
        /**
         * @brief Applies Lemma 1 to the given problem.
         * @param prb Problem instance to transform.
         */
        static void applyLemma1(Kernel::Problem &prb);
};

}

#endif 