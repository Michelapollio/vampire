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
      bool added = false;
      neqPredicate = env.signature->addPredicate("neq", 2, added);
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
//  FO2Logger::logDebug("[Lemma 3] INIZIO APPLICAZIONE LEMMA 3");

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
    unsigned neqPred = state.getNeqPredicate();
    Formula *axiomFormula = createNeqReflexivityAxiom(neqPred);
    Unit *axiomUnit = new FormulaUnit(axiomFormula, Inference(FromInput(UnitInputType::AXIOM)));

    UnitList *axiomWrapper = UnitList::empty();
    UnitList::push(axiomUnit, axiomWrapper);

    // Inject Leibniz congruence axioms for unary predicates: ~P(X0) | P(X1) | neq(X0, X1)
    unsigned predsCount = env.signature->predicates();
    for (unsigned p = 1; p < predsCount; p++) {
      if (p == neqPred) continue;
      Signature::Symbol* sym = env.signature->getPredicate(p);
      if (sym && sym->arity() == 1) {
        TermList varX = TermList::var(0);
        TermList varY = TermList::var(1);
        Lib::Stack<Literal*> lits;
        lits.push(Literal::create1(p, false, varX));
        lits.push(Literal::create1(p, true, varY));
        TermList neqArgs[2] = {varX, varY};
        lits.push(Literal::create(neqPred, 2, true, neqArgs));
        Clause* congCl = Clause::fromStack(lits, Inference(FromInput(UnitInputType::AXIOM)));
        UnitList::push(congCl, axiomWrapper);
      } else if (sym && sym->arity() == 2) {
        TermList varX = TermList::var(0);
        TermList varY = TermList::var(1);
        TermList varZ = TermList::var(2);
        TermList neqArgs[2] = {varX, varY};

        // ~B(X0, X2) | B(X1, X2) | neq(X0, X1)
        Lib::Stack<Literal*> lits1;
        TermList args1_0[2] = {varX, varZ};
        TermList args1_1[2] = {varY, varZ};
        lits1.push(Literal::create(p, 2, false, args1_0));
        lits1.push(Literal::create(p, 2, true, args1_1));
        lits1.push(Literal::create(neqPred, 2, true, neqArgs));
        Clause* congCl1 = Clause::fromStack(lits1, Inference(FromInput(UnitInputType::AXIOM)));
        UnitList::push(congCl1, axiomWrapper);

        // ~B(X2, X0) | B(X2, X1) | neq(X0, X1)
        Lib::Stack<Literal*> lits2;
        TermList args2_0[2] = {varZ, varX};
        TermList args2_1[2] = {varZ, varY};
        lits2.push(Literal::create(p, 2, false, args2_0));
        lits2.push(Literal::create(p, 2, true, args2_1));
        lits2.push(Literal::create(neqPred, 2, true, neqArgs));
        Clause* congCl2 = Clause::fromStack(lits2, Inference(FromInput(UnitInputType::AXIOM)));
        UnitList::push(congCl2, axiomWrapper);
      }
    }

    prb.units() = UnitList::concat(axiomWrapper, prb.units());

    FO2Logger::logDebug("[Lemma 3] Iniettati assiomi di congruenza per neq.");
  }

  FO2Logger::logDebug("[Lemma 3] FINE LEMMA 3");
}

} // namespace FO2Fragment
