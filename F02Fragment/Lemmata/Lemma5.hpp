#ifndef __FO2_LEMMA_5_HPP__
#define __FO2_LEMMA_5_HPP__

#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Clause.hpp"

namespace FO2Fragment {
namespace Lemmata {

class Lemma5 {
public:
  static void applyLemma5(Kernel::Problem &prb);

private:
  static bool isUnaryOnVar(Kernel::Literal* lit, unsigned targetVar);
  static Kernel::Literal* renameVarYtoX(Kernel::Literal* lit);
  static Kernel::Formula* createDisjunctionFromLiterals(const std::vector<Kernel::Literal*>& lits);
  static bool extractLiteralsFromFormula(Kernel::Formula* form, std::vector<Kernel::Literal*>& lits);
  static bool decomposeLiterals(const std::vector<Kernel::Literal*>& lits, std::vector<Kernel::Literal*>& gammaLits, std::vector<Kernel::Literal*>& deltaLits);
  static bool decomposeType3Clause(Kernel::Clause* cl, std::vector<Kernel::Literal*>& gammaLits, std::vector<Kernel::Literal*>& deltaLits);
};

} // namespace Lemmata
} // namespace FO2Fragment

#endif // __FO2_LEMMA_5_HPP__
