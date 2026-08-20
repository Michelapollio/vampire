#include "Lemma6.hpp"
#include "Lemma1.hpp"
#include "Lemma2.hpp"
#include "F02Fragment/FO2Logger.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/Signature.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/OperatorType.hpp"

#include "Lib/Environment.hpp"
#include "Lib/List.hpp"

#include <iostream>
#include <vector>

using namespace Kernel;

namespace FO2Fragment {
namespace Lemmata {

/**
 * @brief Sostituisce ricorsivamente le occorrenze di una variabile con un termine specificato in una formula.
 */
Formula* Lemma6::replaceVarWithTerm(Formula* formula, unsigned varIndex, TermList term)
{
  if (!formula) return nullptr;

  switch (formula->connective()) {
    case LITERAL: {
      Literal* lit = formula->literal();
      unsigned arity = lit->arity();
      if (arity == 0) return formula;

      std::vector<TermList> newArgs;
      newArgs.reserve(arity);
      bool changed = false;

      for (unsigned i = 0; i < arity; i++) {
        TermList arg = *lit->nthArgument(i);
        if (arg.isVar() && arg.var() == varIndex) {
          newArgs.push_back(term);
          changed = true;
        } else {
          newArgs.push_back(arg);
        }
      }

      if (!changed) return formula;

      Literal* newLit = Literal::create(lit->functor(), arity, lit->polarity(), newArgs.data());
      return new AtomicFormula(newLit);
    }

    case NOT: {
      Formula* newArg = replaceVarWithTerm(formula->uarg(), varIndex, term);
      if (newArg != formula->uarg()) {
        return new NegatedFormula(newArg);
      }
      return formula;
    }

    case AND:
    case OR: {
      FormulaList* args = formula->args();
      FormulaList* newArgs = FormulaList::empty();
      bool changed = false;

      FormulaList::Iterator it(args);
      while (it.hasNext()) {
        Formula* arg = it.next();
        Formula* newArg = replaceVarWithTerm(arg, varIndex, term);
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
      Formula* newLeft = replaceVarWithTerm(formula->left(), varIndex, term);
      Formula* newRight = replaceVarWithTerm(formula->right(), varIndex, term);
      if (newLeft != formula->left() || newRight != formula->right()) {
        return new BinaryFormula(formula->connective(), newLeft, newRight);
      }
      return formula;
    }

    case FORALL:
    case EXISTS: {
      Formula* newQarg = replaceVarWithTerm(formula->qarg(), varIndex, term);
      if (newQarg != formula->qarg()) {
        return new QuantifiedFormula(formula->connective(), formula->vars(), newQarg);
      }
      return formula;
    }

    default:
      return formula;
  }
}

/**
 * @brief Cerca ed estrae la sottoformula zeta(x) da una struttura di quantificatore d'unicita' (exists! x zeta(x)).
 */
Formula* Lemma6::extractZetaFromUniqueness(Formula* formula)
{
  if (!formula) return nullptr;

  if (formula->connective() == EXISTS) {
    Formula* body = formula->qarg();
    if (body && body->connective() == AND) {
      bool hasForall = false;
      Formula* candidateZeta = nullptr;
      FormulaList::Iterator it(body->args());
      while (it.hasNext()) {
        Formula* child = it.next();
        if (child->connective() == FORALL) {
          hasForall = true;
        } else {
          candidateZeta = child;
        }
      }
      if (hasForall && candidateZeta) {
        return candidateZeta;
      }
    }
  }

  switch (formula->connective()) {
    case NOT:
      return extractZetaFromUniqueness(formula->uarg());
    case AND:
    case OR: {
      FormulaList::Iterator it(formula->args());
      while (it.hasNext()) {
        Formula* res = extractZetaFromUniqueness(it.next());
        if (res) return res;
      }
      break;
    }
    case IMP:
    case IFF:
    case XOR:
      break;
    case FORALL:
      return extractZetaFromUniqueness(formula->qarg());
    default:
      break;
  }

  return nullptr;
}

/**
 * @brief Sostituisce la sottoformula di quantificazione d'unicita' con la costante vera (TRUE) all'interno di una formula.
 */
Formula* Lemma6::replaceUniquenessWithFormula(Formula* formula, Formula* replacement)
{
  if (!formula) return nullptr;

  if (formula->connective() == EXISTS) {
    Formula* body = formula->qarg();
    if (body && body->connective() == AND) {
      bool hasForall = false;
      FormulaList::Iterator it(body->args());
      while (it.hasNext()) {
        if (it.next()->connective() == FORALL) {
          hasForall = true;
          break;
        }
      }
      if (hasForall) {
        return replacement;
      }
    }
  }

  switch (formula->connective()) {
    case NOT: {
      Formula* newArg = replaceUniquenessWithFormula(formula->uarg(), replacement);
      if (newArg != formula->uarg()) return new NegatedFormula(newArg);
      return formula;
    }
    case AND:
    case OR: {
      FormulaList* args = formula->args();
      FormulaList* newArgs = FormulaList::empty();
      bool changed = false;

      FormulaList::Iterator it(args);
      while (it.hasNext()) {
        Formula* arg = it.next();
        Formula* newArg = replaceUniquenessWithFormula(arg, replacement);
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
      Formula* newLeft = replaceUniquenessWithFormula(formula->left(), replacement);
      Formula* newRight = replaceUniquenessWithFormula(formula->right(), replacement);
      if (newLeft != formula->left() || newRight != formula->right()) {
        return new BinaryFormula(formula->connective(), newLeft, newRight);
      }
      return formula;
    }
    case FORALL: {
      Formula* newQarg = replaceUniquenessWithFormula(formula->qarg(), replacement);
      if (newQarg != formula->qarg()) {
        return new QuantifiedFormula(formula->connective(), formula->vars(), newQarg);
      }
      return formula;
    }
    default:
      return formula;
  }
}

/**
 * @brief Genera gli assiomi di congruenza theta_i per la costante e_i e la formula unaria zeta_i(x).
 */
void Lemma6::generateCongruenceAxioms(Formula* zetaI, TermList constTerm, FormulaList* &axioms)
{
  const TermList sort = AtomicSort::defaultSort();

  Formula* zetaEi = replaceVarWithTerm(zetaI, 0, constTerm);
  FormulaList::push(zetaEi, axioms);

  unsigned numPreds = env.signature->predicates();
  for (unsigned p = 1; p < numPreds; p++) {
    if (env.signature->isEqualityPredicate(p)) continue;

    Signature::Symbol* sym = env.signature->getPredicate(p);
    unsigned arity = sym->arity();

    if (arity == 1) {
      Literal* pXLit = Literal::create1(p, true, TermList::var(0));
      Literal* pEiLit = Literal::create1(p, true, constTerm);

      Formula* pX = new AtomicFormula(pXLit);
      Formula* pEi = new AtomicFormula(pEiLit);

      Formula* iffForm = new BinaryFormula(IFF, pX, pEi);
      Formula* impForm = new BinaryFormula(IMP, zetaI, iffForm);
      Formula* forallForm = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), impForm);

      FormulaList::push(forallForm, axioms);
    }
    else if (arity == 2) {
      TermList argsXY[2] = {TermList::var(0), TermList::var(1)};
      TermList argsEiY[2] = {constTerm, TermList::var(1)};
      Literal* qXYLit = Literal::create(p, 2, true, argsXY);
      Literal* qEiYLit = Literal::create(p, 2, true, argsEiY);

      Formula* qXY = new AtomicFormula(qXYLit);
      Formula* qEiY = new AtomicFormula(qEiYLit);

      Formula* iffForm1 = new BinaryFormula(IFF, qXY, qEiY);
      Formula* impForm1 = new BinaryFormula(IMP, zetaI, iffForm1);
      Formula* forallY1 = new QuantifiedFormula(FORALL, VSList::singleton({1u, sort}), impForm1);
      Formula* forallX1 = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), forallY1);

      FormulaList::push(forallX1, axioms);

      TermList argsYX[2] = {TermList::var(1), TermList::var(0)};
      TermList argsYEi[2] = {TermList::var(1), constTerm};
      Literal* qYXLit = Literal::create(p, 2, true, argsYX);
      Literal* qYEiLit = Literal::create(p, 2, true, argsYEi);

      Formula* qYX = new AtomicFormula(qYXLit);
      Formula* qYEi = new AtomicFormula(qYEiLit);

      Formula* iffForm2 = new BinaryFormula(IFF, qYX, qYEi);
      Formula* impForm2 = new BinaryFormula(IMP, zetaI, iffForm2);
      Formula* forallY2 = new QuantifiedFormula(FORALL, VSList::singleton({1u, sort}), impForm2);
      Formula* forallX2 = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), forallY2);

      FormulaList::push(forallX2, axioms);
    }
  }
}

/**
 * @brief Applica il Lemma 6 sostituendo i quantificatori d'unicita' con assiomi di congruenza su nuove costanti.
 */
void Lemma6::applyLemma6(Problem &prb)
{
  FO2Logger::logPhase("Inizio Lemma 6: Eliminazione Quantificatori d'Unicita'");

  UnitList* newUnits = UnitList::empty();
  bool createdConstants = false;

  UnitList::DelIterator it(prb.units());
  while (it.hasNext()) {
    Unit* unit = it.next();
    if (!unit->isClause()) {
      FormulaUnit* fu = static_cast<FormulaUnit*>(unit);
      Formula* zetaI = extractZetaFromUniqueness(fu->formula());

      if (zetaI) {
        createdConstants = true;
        unsigned freshConstFunctor = env.signature->addFreshFunction(0, "e_");
        env.signature->getFunction(freshConstFunctor)->setType(OperatorType::getConstantsType(AtomicSort::defaultSort()));
        TermList constTerm = TermList(Term::createConstant(freshConstFunctor));

        // Normalize all variables in zetaI to variable 0 (X0) to maintain 2-variable bound
        Formula* normalizedZeta = zetaI;
        for (unsigned v = 1; v < 16; ++v) {
          normalizedZeta = replaceVarWithTerm(normalizedZeta, v, TermList::var(0));
        }

        FO2Logger::logDebug("[Lemma 6] Trovata asserzione d'unicita' per zeta(x): " + normalizedZeta->toString());
        FO2Logger::logDebug("[Lemma 6] Generata costante fresca: " + env.signature->getFunction(freshConstFunctor)->name());

        FormulaList* axioms = FormulaList::empty();
        generateCongruenceAxioms(normalizedZeta, constTerm, axioms);

        Formula* axiomsConj = JunctionFormula::generalJunction(AND, axioms);
        Formula* cleanFormula = replaceUniquenessWithFormula(fu->formula(), axiomsConj);
        FormulaUnit* cleanUnit = new FormulaUnit(cleanFormula, Inference(FromInput(UnitInputType::AXIOM)));
        UnitList::push(cleanUnit, newUnits);

        continue;
      }
    }

    UnitList::push(unit, newUnits);
  }

  prb.units() = UnitList::reverse(newUnits);

  if (createdConstants) {
    FO2Logger::logDebug("[Lemma 6] Riapplicazione Lemma 1 per eliminare le costanti fresche e tornare a L2 puro...");
    Lemma1::applyLemma1(prb);
    FO2Logger::logDebug("[Lemma 6] Riapplicazione Lemma 2 per garantire la forma normale di Scott a 2 variabili...");
    Lemma2::applyLemma2(prb);
  }

  FO2Logger::logPhase("Lemma 6 Completato: Formula ridotta a L2 puro (senza uguaglianza)");
}

} // namespace Lemmata
} // namespace FO2Fragment
