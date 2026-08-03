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
 * Determina il tipo di Scott (Tipo 1, Tipo 2, Tipo 3) di una formula.
 */
ScottType determineScottType(Kernel::Formula *formula);

} // namespace FO2Fragment

#endif // FO2_SCOTT_TYPES_HPP
