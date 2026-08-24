#ifndef FO2_SCOTT_TYPES_HPP
#define FO2_SCOTT_TYPES_HPP

#include "Kernel/Formula.hpp"

namespace FO2Fragment {

/**
 * Tipi di Formule secondo la Forma Normale di Scott (Lemmi 2, 3, 4, 5, 6).
 * - TYPE_1: Ex. alpha_i(x)
 * - TYPE_2: Ax Ey. alpha_i(x,y)
 * - TYPE_3: Ax Ay. alpha_i(x,y)
 */
enum class ScottType {
  TYPE_1,
  TYPE_2,
  TYPE_3
};

/**
 * @brief Determines the Scott Normal Form type (Type 1, Type 2, or Type 3) of a given formula.
 * @param formula Pointer to the formula to inspect.
 * @return ScottType classification (TYPE_1, TYPE_2, or TYPE_3).
 */
ScottType determineScottType(Kernel::Formula *formula);

} // namespace FO2Fragment

#endif // FO2_SCOTT_TYPES_HPP
