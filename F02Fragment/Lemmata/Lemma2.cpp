#include "Lemma2.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/SortHelper.hpp"

#include "Lib/DHMap.hpp"
#include "Lib/Stack.hpp"
#include "Lib/Environment.hpp"
#include "F02Fragment/FO2Logger.hpp"

#include <iostream>
#include <string>

using namespace Kernel;

namespace FO2Fragment {

namespace {

void getFreeVars(Formula *formula, bool &hasX, bool &hasY)
{
  if (!formula)
    return;

  switch (formula->connective()) {
    case LITERAL: {
      Literal *lit = formula->literal();
      unsigned ar = lit->arity();
      for (unsigned i = 0; i < ar; ++i) {
        TermList arg = *lit->nthArgument(i);
        if (arg.isVar()) {
          if (arg.var() == 0)
            hasX = true;
          if (arg.var() == 1)
            hasY = true;
        }
      }
      break;
    }

    case NOT:
      getFreeVars(formula->uarg(), hasX, hasY);
      break;

    case IMP:
    case IFF:
    case XOR:
      getFreeVars(formula->left(), hasX, hasY);
      getFreeVars(formula->right(), hasX, hasY);
      break;

    case AND:
    case OR: {
      FormulaList::Iterator it(formula->args());
      while (it.hasNext()) {
        getFreeVars(it.next(), hasX, hasY);
      }
      break;
    }

    case FORALL:
    case EXISTS: {
      bool subX = false;
      bool subY = false; 
      getFreeVars(formula->qarg(), subX, subY);
      break;
    }

    default:
      break;
  }
}

Formula *renameFormula(Formula *formula, Stack<Formula *> &newDefinitions)
{
  if (!formula)
    return nullptr;

  switch (formula->connective()) {
    case LITERAL:
      return formula;

    case NOT: {
      Formula *newArg = renameFormula(formula->uarg(), newDefinitions);
      if (newArg != formula->uarg()) {
        return new NegatedFormula(newArg);
      }

      return formula;
    }

    case IMP:
    case IFF:
    case XOR: {
      Formula *newLeft = renameFormula(formula->left(), newDefinitions);
      Formula *newRight = renameFormula(formula->right(), newDefinitions);
      if (newLeft != formula->left() || newRight != formula->right()) {
        return new BinaryFormula(formula->connective(), newLeft, newRight);
      }
      return formula;
    }

    case AND:
    case OR: {
      FormulaList *args = formula->args();
      FormulaList *newArgs = FormulaList::empty();
      bool changed = false;

      FormulaList::Iterator it(args);
      while (it.hasNext()) {
        Formula *arg = it.next();
        Formula *newArg = renameFormula(arg, newDefinitions);
        if (newArg != arg)
          changed = true;
        FormulaList::push(newArg, newArgs);
      }
      newArgs = FormulaList::reverse(newArgs);

      if (changed) {
        return new JunctionFormula(formula->connective(), newArgs);
      }

      FormulaList::destroy(newArgs);
      return formula;
    }

    case FORALL:
    case EXISTS: {
      Formula *processedSubf = renameFormula(formula->qarg(), newDefinitions);

      bool hasX = false;
      bool hasY = false;
      getFreeVars(processedSubf, hasX, hasY);

      if (formula->connective() == EXISTS && processedSubf->connective() != LITERAL) {
        FO2Logger::logDebug("[Lemma 2] sottoformula complessa : " + processedSubf->toString());

        unsigned arity = 0;
        if (hasX) arity++;
        if (hasY) arity++;

        unsigned newPred = env.signature->addFreshPredicate(arity, "p_def");
        const TermList sort = AtomicSort::defaultSort();

        Literal *newLit = nullptr;
        if (arity == 1) {
          TermList var = hasX ? TermList::var(0) : TermList::var(1);
          newLit = Literal::create1(newPred, true, var);
        } else if (arity == 2) {
          TermList args[2] = {TermList::var(0), TermList::var(1)};
          newLit = Literal::create(newPred, arity, true, args);
        } else {
          newLit = Literal::create(newPred, true, {});
        }

        // Costruiamo la formula di definizione: ∀[vars]. (p_def(vars) <=> sottoformula)
        Formula* defAtom = new AtomicFormula(newLit);
        Formula* biconditional = new BinaryFormula(IFF, defAtom, processedSubf);

        Formula* defFormula = biconditional;
        if (hasY) {
          defFormula = new QuantifiedFormula(FORALL, VSList::singleton({1u, sort}), defFormula);
        }
        if (hasX) {
          defFormula = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), defFormula);
        }

        newDefinitions.push(defFormula);

        // Sostituiamo la sottoformula complessa con il nuovo atomo
        Formula *replacementAtom = new AtomicFormula(newLit);
        return new QuantifiedFormula(formula->connective(), formula->vars(), replacementAtom);
      }

      if (processedSubf != formula->qarg()) {
        return new QuantifiedFormula(formula->connective(), formula->vars(), processedSubf);
      }
      return formula;
    }
    default:
      return formula;
  }
  return formula;
}

} // namespace

void Lemma2::applyLemma2(Kernel::Problem &prb)
{

  FO2Logger::logDebug("[Lemma 2] INIZIO APPLICAZIONE LEMMA 2");

  Stack<Formula *> newDefinitions;

  UnitList::DelIterator it(prb.units());
  while (it.hasNext()) {
    Unit *unit = it.next();

    if (!unit->isClause()) {
      FormulaUnit *fu = static_cast<FormulaUnit *>(unit);
      Formula *originFormula = fu->formula();

      Formula *processedFormula = renameFormula(originFormula, newDefinitions);

      if (processedFormula != originFormula) {
        FormulaUnit *newUnit = new FormulaUnit(processedFormula, NonspecificInference1(InferenceRule::INPUT, unit));
        it.replace(newUnit);
      }
    }
  }

  if (!newDefinitions.isEmpty()) {
    UnitList *newUnits = UnitList::empty();

    while (!newDefinitions.isEmpty()) {
      Formula *defFormula = newDefinitions.pop();
      Unit *defUnit = new FormulaUnit(defFormula, Inference(FromInput(UnitInputType::AXIOM)));
      UnitList::push(defUnit, newUnits);
    }
    prb.units() = UnitList::concat(newUnits, prb.units());
  }

  FO2Logger::logDebug("[Lemma 2] FINE LEMMA 2");
}

} // namespace FO2Fragment