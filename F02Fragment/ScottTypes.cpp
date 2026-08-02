#include "ScottTypes.hpp"

using namespace Kernel;

namespace FO2Fragment {

ScottType determineScottType(Formula *formula)
{
  if (!formula) return ScottType::TYPE_3;

  if (formula->connective() == EXISTS) {
    return ScottType::TYPE_1;
  }
  if (formula->connective() == FORALL) {
    Formula *qbody = formula->qarg();
    if (qbody && qbody->connective() == EXISTS) {
      return ScottType::TYPE_2;
    }
  }

  return ScottType::TYPE_3;
}

} // namespace FO2Fragment
