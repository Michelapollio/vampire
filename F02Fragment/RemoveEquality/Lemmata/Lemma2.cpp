#include "Lemma2.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/Inference.hpp"

#include "Lib/DHMap.hpp"
#include "Lib/Stack.hpp"
#include "Lib/Environment.hpp"
#include "F02Fragment/FO2Logger.hpp"

#include <iostream>
#include <string>

using namespace Kernel;

namespace FO2Fragment {

namespace {

void collectFreeVars(const Formula *formula, Kernel::DHSet<unsigned> &freeVars)
{
  if (!formula)
    return;

  switch (formula->connective()) {
    case LITERAL: {
      const Literal *lit = formula->literal();
      unsigned ar = lit->arity();
      for (unsigned i = 0; i < ar; ++i) {
        TermList arg = *lit->nthArgument(i);
        if (arg.isVar()) {
          freeVars.insert(arg.var());
        }
      }
      break;
    }

    case NOT:
      collectFreeVars(formula->uarg(), freeVars);
      break;

    case IMP:
    case IFF:
    case XOR:
      collectFreeVars(formula->left(), freeVars);
      collectFreeVars(formula->right(), freeVars);
      break;

    case AND:
    case OR: {
      FormulaList::Iterator it(formula->args());
      while (it.hasNext()) {
        collectFreeVars(it.next(), freeVars);
      }
      break;
    }

    case FORALL:
    case EXISTS: {
      Kernel::DHSet<unsigned> subVars;
      collectFreeVars(formula->qarg(), subVars);

      Kernel::VSList::Iterator vit(formula->vars());
      while (vit.hasNext()) {
        unsigned bVar = vit.next().first; 
        subVars.remove(bVar);
      }

      Kernel::DHSet<unsigned>::Iterator sit(subVars);
      while (sit.hasNext()) {
        freeVars.insert(sit.next());
      }
      break;
    }

    default:
      break;
  }
}

Formula *renameFormula(Formula *formula, Stack<Formula *> &newDefinitions, bool isNegated = false)
{
  if (!formula)
    return nullptr;

  switch (formula->connective()) {
    case LITERAL:
      return formula;

    case NOT: {
      Formula *newArg = renameFormula(formula->uarg(), newDefinitions, !isNegated);
      if (newArg != formula->uarg()) {
        return new NegatedFormula(newArg);
      }
      return formula;
    }

    case IMP: {
      Formula *newLeft = renameFormula(formula->left(), newDefinitions, !isNegated);
      Formula *newRight = renameFormula(formula->right(), newDefinitions, isNegated);
      if (newLeft != formula->left() || newRight != formula->right()) {
        return new BinaryFormula(formula->connective(), newLeft, newRight);
      }
      return formula;
    }

    case IFF:
    case XOR: {
      Formula *newLeft = renameFormula(formula->left(), newDefinitions, false);
      Formula *newRight = renameFormula(formula->right(), newDefinitions, false);
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
        Formula *newArg = renameFormula(arg, newDefinitions, isNegated);
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
      Formula *processedSubf = renameFormula(formula->qarg(), newDefinitions, isNegated);

      if (formula->connective() == EXISTS && processedSubf->connective() != LITERAL && !isNegated) {
        if (processedSubf->connective() == AND) {
          bool containsForall = false;
          FormulaList::Iterator it(processedSubf->args());
          while (it.hasNext()) {
            if (it.next()->connective() == FORALL) {
              containsForall = true;
              break;
            }
          }
          if (containsForall) {
            if (processedSubf != formula->qarg()) {
              return new QuantifiedFormula(formula->connective(), formula->vars(), processedSubf);
            }
            return formula;
          }
        }

        Kernel::DHSet<unsigned> freeVars;
        collectFreeVars(processedSubf, freeVars);

        Kernel::VSList::Iterator vit(formula->vars());
        while (vit.hasNext()) {
          unsigned bVar = vit.next().first;
          freeVars.remove(bVar);
        }

        unsigned arity = freeVars.size();

        if (arity > 1) {
          if (processedSubf != formula->qarg()) {
            return new QuantifiedFormula(formula->connective(), formula->vars(), processedSubf);
          }
          return formula;
        }

        unsigned newPred = env.signature->addFreshPredicate(arity, "p_def");
        const TermList sort = AtomicSort::defaultSort();

        Literal *newLit = nullptr;
        if (arity == 1) {
          Kernel::DHSet<unsigned>::Iterator it(freeVars);
          it.hasNext(); // CRUCIAL: MUST BE CALLED BEFORE next() IN VAMPIRE
          unsigned varIdx = it.next();
          newLit = Literal::create1(newPred, true, TermList::var(varIdx));
        } else {
          newLit = Literal::create(newPred, true, {});
        }

        Formula* defAtom = new AtomicFormula(newLit);
        Formula* biconditional = new BinaryFormula(IFF, defAtom, new QuantifiedFormula(formula->connective(), formula->vars(), processedSubf));

        Formula* defFormula = biconditional;
        Kernel::DHSet<unsigned>::Iterator fit(freeVars);
        while (fit.hasNext()) {
          unsigned varIdx = fit.next();
          defFormula = new QuantifiedFormula(FORALL, VSList::singleton({varIdx, sort}), defFormula);
        }

        newDefinitions.push(defFormula);

        // Crucial fix: return ONLY the replacement atom.
        return defAtom;
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
  FO2Logger::logDebug("[Lemma 2] INIZIO LEMMA 2");

  Stack<Formula *> newDefinitions;

  Kernel::UnitList::DelIterator it(prb.units());
  while (it.hasNext()) {
    Unit *unit = it.next();

    if (unit->isClause())
      continue;

    FormulaUnit *formulaUnit = static_cast<FormulaUnit *>(unit);
    Formula *originFormula = formulaUnit->formula();

    if (originFormula) {
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
