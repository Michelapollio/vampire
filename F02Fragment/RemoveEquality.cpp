#include "RemoveEquality.hpp"


#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/SortHelper.hpp"

#include "Lib/DHSet.hpp"
#include "Lib/List.hpp"

#include "Shell/EqualityProxy.hpp"
#include "Shell/EqualityProxyMono.hpp"
#include "Shell/Options.hpp"

#include "Lib/Environment.hpp"

#include "Lemmata/Lemma1.hpp"
#include "Lemmata/Lemma2.hpp"
#include "Lemmata/Lemma3.hpp"

#include <iostream>
#include <string>

using namespace Kernel;
using namespace Shell;

namespace FO2Fragment {

void RemoveEquality::Lemma1Application(Kernel::Problem &prb){
  
  FO2Fragment::Lemma1::applyLemma1(prb);
}

void RemoveEquality::Lemma2Application(Kernel::Problem &prb)
{
  FO2Fragment::Lemma2::applyLemma2(prb);
}

void RemoveEquality::Lemma3Application(Kernel::Problem &prb)
{
  FO2Fragment::Lemma3::applyLemma3(prb);
}

void RemoveEquality::Lemma4Application(Kernel::Problem &prb)
{
  (void)prb;
}

void RemoveEquality::Lemma5Application(Kernel::Problem &prb)
{
  (void)prb;
}

void RemoveEquality::Lemma6Application(Kernel::Problem &prb)
{
  (void)prb;
}

void RemoveEquality::removeEquality(Kernel::Problem &prb)
{
  if (!prb.hasEquality()) {
    return;
  }
  std::cout << "--- PRIMA DEL LEMMA 1 ---" << std::endl;
  for (UnitList::Iterator it(prb.units()); it.hasNext();) {
    std::cout << it.next()->toString() << std::endl;
  }
  Lemma1Application(prb);
  std::cout << "--- DOPO IL LEMMA 1 ---" << std::endl;
  for (UnitList::Iterator it(prb.units()); it.hasNext();) {
      std::cout << it.next()->toString() << std::endl;
  }
  Lemma2Application(prb);
  std::cout << "--- DOPO IL LEMMA 2 ---" << std::endl;
  for (UnitList::Iterator it(prb.units()); it.hasNext();) {
      std::cout << it.next()->toString() << std::endl;
  }
  Lemma3Application(prb);
  std::cout << "--- DOPO IL LEMMA 3 ---" << std::endl;
  for (UnitList::Iterator it(prb.units()); it.hasNext();) {
      std::cout << it.next()->toString() << std::endl;
  }
  Lemma4Application(prb);
  Lemma5Application(prb);
  Lemma6Application(prb);
}

void RemoveEquality::traceProblem(Kernel::Problem &prb)
{
  (void)prb;
}

void RemoveEquality::traceFormula(Kernel::Formula *formula, int depth)
{
  (void)formula; (void)depth;
}

void RemoveEquality::traceClause(Kernel::Clause *cl)
{
  (void)cl;
}

} // namespace FO2Fragment
