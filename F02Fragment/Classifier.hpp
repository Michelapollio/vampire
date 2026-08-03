#include "Kernel/Unit.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Clause.hpp"

namespace FO2Fragment {
using namespace Kernel;

class Classifier {

public:
  /**
   * Analizza l'intera UnitList restituita dal parser
   * Ritorna true se il problema appartiene al frammento a due variabili
   * e imposta `hasEq` se sono presenti letterali di uguaglianza
   */
    static bool isFO2(UnitList *ul, bool &hasEq);

private:
  /**
   * Analisi ricorsiva per formula (FOF)
   */
  static bool isFO2Formula(Formula *f, DHSet<unsigned> &vars, bool &hasEq);

  /**
   * Analisi per le clausole (CNF)
   */
  static bool isFO2Clause(Clause *clause, DHSet<unsigned> &vars, bool &hasEq);
};
} // namespace FO2Fragment