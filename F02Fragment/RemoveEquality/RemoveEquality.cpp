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
#include "Lemmata/Lemma4.hpp"
#include "Lemmata/Lemma5.hpp"
#include "Lemmata/Lemma6.hpp"
#include "F02Fragment/Classifier/Classifier.hpp"
#include "F02Fragment/FO2Logger.hpp"

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
  FO2Fragment::Lemmata::Lemma4::applyLemma4(prb);
}

void RemoveEquality::Lemma5Application(Kernel::Problem &prb)
{
  FO2Fragment::Lemmata::Lemma5::applyLemma5(prb);
}

void RemoveEquality::Lemma6Application(Kernel::Problem &prb)
{
  FO2Fragment::Lemmata::Lemma6::applyLemma6(prb);
}

void RemoveEquality::removeEquality(Kernel::Problem &prb)
{
  bool hasEq = false;
  bool isFO2 = Classifier::isFO2(prb.units(), hasEq);

  if (!isFO2) {
    FO2Logger::logPhase("Il problema NON appartiene al frammento FO2 (più di 2 variabili libere o quantificate).");
    return;
  }

  if (!hasEq && !prb.hasEquality()) {
    FO2Logger::logPhase("Il problema appartiene al frammento FO2 e NON contiene uguaglianze. I lemmata vengono saltati.");
    return;
  }

  FO2Logger::logPhase("Inizio procedura di rimozione dell'uguaglianza (RemoveEquality)");

  FO2Logger::logLemma("PRIMA DEL LEMMA 1", prb);
  Lemma1Application(prb);

  FO2Logger::logLemma("DOPO IL LEMMA 1", prb);
  Lemma2Application(prb);

  FO2Logger::logLemma("DOPO IL LEMMA 2", prb);
  Lemma3Application(prb);

  FO2Logger::logLemma("DOPO IL LEMMA 3", prb);
  Lemma4Application(prb);

  FO2Logger::logLemma("DOPO IL LEMMA 4", prb);
  Lemma5Application(prb);

  FO2Logger::logLemma("DOPO IL LEMMA 5", prb);
  Lemma6Application(prb);

  FO2Logger::logLemma("DOPO IL LEMMA 6", prb);

  FO2Logger::logPhase("Rimozione dell'uguaglianza completata");
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
