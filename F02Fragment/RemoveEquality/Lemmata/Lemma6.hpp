#ifndef __FO2_LEMMA_6_HPP__
#define __FO2_LEMMA_6_HPP__

#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Clause.hpp"

namespace FO2Fragment {
namespace Lemmata {

class Lemma6 {
public:
  static void applyLemma6(Kernel::Problem &prb);

private:
  static Kernel::Formula* replaceVarWithTerm(Kernel::Formula* formula, unsigned varIndex, Kernel::TermList term);
  static Kernel::Formula* extractZetaFromUniqueness(Kernel::Formula* formula);
  static Kernel::Formula* replaceUniquenessWithFormula(Kernel::Formula* formula, Kernel::Formula* replacement);
  static void generateCongruenceAxioms(Kernel::Formula* zetaI, Kernel::TermList constTerm, Kernel::FormulaList* &axioms);
};

} // namespace Lemmata
} // namespace FO2Fragment

#endif // __FO2_LEMMA_6_HPP__
