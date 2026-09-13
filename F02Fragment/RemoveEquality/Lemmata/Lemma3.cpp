#include "Lemma3.hpp"
#include "F02Fragment/ScottTypes.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/Signature.hpp"

#include "Lib/DHMap.hpp"
#include "Lib/List.hpp"
#include "Lib/Environment.hpp"
#include "Lib/Stack.hpp"
#include "F02Fragment/FO2Logger.hpp"

#include <iostream>
#include <string>

using namespace Kernel;

namespace FO2Fragment {
namespace {

/**
 * @brief Classifica un letterale di uguaglianza per il Lemma 3.
 */
enum class EqualityType {
  NOT_EQUALITY,
  TAUTOLOGY_TRUE,
  CONTRADICTION_FALSE,
  POS_DISTINCT_EQ,
  NEG_DISTINCT_EQ
};

/**
 * @brief Analizza e classifica un letterale rispetto all'uguaglianza.
 */
EqualityType classifyEqualityLiteral(const Literal *lit)
{
  if (!lit || !lit->isEquality()) {
    return EqualityType::NOT_EQUALITY;
  }

  const TermList arg0 = *lit->nthArgument(0);
  const TermList arg1 = *lit->nthArgument(1);

  if (arg0.isVar() && arg1.isVar()) {
    unsigned var0 = arg0.var();
    unsigned var1 = arg1.var();

    if (var0 == var1) {
      return lit->isPositive() ? EqualityType::TAUTOLOGY_TRUE 
                               : EqualityType::CONTRADICTION_FALSE;
    }

    return lit->isPositive() ? EqualityType::POS_DISTINCT_EQ 
                             : EqualityType::NEG_DISTINCT_EQ;
  }

  return EqualityType::NOT_EQUALITY;
}

/**
 * @brief Gestisce lo stato globale del Lemma 3 per la creazione del predicato neq.
 */
struct Lemma3State {
  unsigned neqPredicate = 0;
  bool requiresNeqAxiom = false;

  unsigned getNeqPredicate() {
    if (neqPredicate == 0) {
      neqPredicate = env.signature->addFreshPredicate(2, "neq");
      requiresNeqAxiom = true;
    }
    return neqPredicate;
  }
};

/**
 * @brief Costruisce l'assioma di anti-riflessività per neq: ∀x (¬neq(x, x)).
 */
Formula *createNeqReflexivityAxiom(unsigned neqFunctor)
{
  const TermList sort = AtomicSort::defaultSort();
  TermList varX = TermList::var(0);
  TermList args[2] = {varX, varX};

  Literal *neqXXLit = Literal::create(neqFunctor, 2, true, args);
  Formula *neqXXAtom = new AtomicFormula(neqXXLit);
  Formula *notNeqXX = new NegatedFormula(neqXXAtom);

  return new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), notNeqXX);
}

/**
 * @brief Trasforma ricorsivamente qualsiasi formula FOF sostituendo le disuguaglianze x != y con neq(x,y).
 */
Formula *replaceEqualityInFormula(Formula *formula, Lemma3State &state)
{
  if (!formula) return nullptr;

  switch (formula->connective()) {
    case LITERAL: {
      Literal *lit = formula->literal();
      EqualityType eqType = classifyEqualityLiteral(lit);
      if (eqType == EqualityType::NEG_DISTINCT_EQ) {
        unsigned neqFunctor = state.getNeqPredicate();
        TermList args[2] = {*lit->nthArgument(0), *lit->nthArgument(1)};
        Literal *neqLit = Literal::create(neqFunctor, 2, true, args);
        return new AtomicFormula(neqLit);
      }
      return formula;
    }

    case NOT: {
      Formula *uarg = formula->uarg();
      if (uarg && uarg->connective() == LITERAL) {
        const Literal *baseLit = uarg->literal();
        Literal *negLit = Literal::create(const_cast<Literal*>(baseLit), false);
        EqualityType eqType = classifyEqualityLiteral(negLit);
        if (eqType == EqualityType::NEG_DISTINCT_EQ) {
          unsigned neqFunctor = state.getNeqPredicate();
          TermList args[2] = {*negLit->nthArgument(0), *negLit->nthArgument(1)};
          Literal *neqLit = Literal::create(neqFunctor, 2, true, args);
          return new AtomicFormula(neqLit);
        }
      }
      Formula *newArg = replaceEqualityInFormula(uarg, state);
      if (newArg != uarg) {
        return new NegatedFormula(newArg);
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
        Formula *newArg = replaceEqualityInFormula(arg, state);
        if (newArg != arg) changed = true;
        FormulaList::push(newArg, newArgs);
      }
      newArgs = FormulaList::reverse(newArgs);

      if (changed) {
        return JunctionFormula::generalJunction(formula->connective(), newArgs);
      }
      FormulaList::destroy(newArgs);
      return formula;
    }

    case IMP:
    case IFF:
    case XOR: {
      Formula *newLeft = replaceEqualityInFormula(formula->left(), state);
      Formula *newRight = replaceEqualityInFormula(formula->right(), state);
      if (newLeft != formula->left() || newRight != formula->right()) {
        return new BinaryFormula(formula->connective(), newLeft, newRight);
      }
      return formula;
    }

    case FORALL:
    case EXISTS: {
      Formula *newArg = replaceEqualityInFormula(formula->qarg(), state);
      if (newArg != formula->qarg()) {
        return new QuantifiedFormula(formula->connective(), formula->vars(), newArg);
      }
      return formula;
    }

    default:
      return formula;
  }
}

} // namespace

/**
 * @brief Applica il Lemma 3 al problema trasformando le uguaglianze ed isolandole.
 */
void Lemma3::applyLemma3(Kernel::Problem &prb)
{
  FO2Logger::logDebug("[Lemma 3] INIZIO APPLICAZIONE LEMMA 3");

  Lemma3State state;

  UnitList::DelIterator it(prb.units());
  while (it.hasNext()) {
    Unit *unit = it.next();

    if (!unit->isClause()) {
      FormulaUnit *fu = static_cast<FormulaUnit *>(unit);
      Formula *formula = fu->formula();

      Formula *cleanFormula = replaceEqualityInFormula(formula, state);
      if (cleanFormula && cleanFormula != formula) {
        FormulaUnit *newUnit = new FormulaUnit(cleanFormula, NonspecificInference1(InferenceRule::INPUT, unit));
        it.replace(newUnit);
      }
    }
  }

  if (state.requiresNeqAxiom) {
    Formula *axiomFormula = createNeqReflexivityAxiom(state.getNeqPredicate());
    Unit *axiomUnit = new FormulaUnit(axiomFormula, Inference(FromInput(UnitInputType::AXIOM)));

    UnitList *axiomWrapper = UnitList::empty();
    UnitList::push(axiomUnit, axiomWrapper);
    prb.units() = UnitList::concat(axiomWrapper, prb.units());

    FO2Logger::logDebug("[Lemma 3] Iniettato assioma di anti-riflessivita' per neq: " + axiomFormula->toString());
  }

  FO2Logger::logDebug("[Lemma 3] FINE LEMMA 3");
}

} // namespace FO2Fragment
