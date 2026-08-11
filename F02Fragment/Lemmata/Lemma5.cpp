#include "Lemma5.hpp"
#include "F02Fragment/ScottTypes.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/TermIterators.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/Inference.hpp"
#include "Lib/Stack.hpp"

#include <iostream>
#include <vector>

using namespace Kernel;

namespace FO2Fragment {
namespace Lemmata {

/**
 * @brief Controlla se un letterale contiene esclusivamente la variabile specificata (targetVar) o nessuna variabile.
 */
bool Lemma5::isUnaryOnVar(Kernel::Literal* lit, unsigned targetVar)
{
  if (!lit || lit->isEquality()) {
    return false;
  }

  bool hasTarget = false;
  bool hasOther = false;

  Kernel::VariableIterator vit(lit);
  while (vit.hasNext()) {
    Kernel::TermList tl = vit.next();
    unsigned var = tl.var();
    if (var == targetVar) {
      hasTarget = true;
    } else {
      hasOther = true;
    }
  }

  return !hasOther;
}

/**
 * @brief Rinombra la variabile 1 (Y) in 0 (X) all'interno di un letterale unario.
 */
Kernel::Literal* Lemma5::renameVarYtoX(Kernel::Literal* lit)
{
  if (!lit || lit->isEquality()) {
    return lit;
  }

  unsigned arity = lit->arity();
  if (arity == 0) {
    return lit;
  }

  std::vector<Kernel::TermList> args;
  args.reserve(arity);
  for (unsigned i = 0; i < arity; i++) {
    Kernel::TermList arg = *lit->nthArgument(i);
    if (arg.isVar() && arg.var() == 1) {
      args.push_back(Kernel::TermList::var(0));
    } else {
      args.push_back(arg);
    }
  }

  return Kernel::Literal::create(lit->functor(), arity, lit->polarity(), args.data());
}

/**
 * @brief Crea una formula di disgiunzione (OR) a partire da un vettore di letterali.
 */
Kernel::Formula* Lemma5::createDisjunctionFromLiterals(const std::vector<Kernel::Literal*>& lits)
{
  if (lits.empty()) {
    return new Kernel::Formula(false);
  }

  if (lits.size() == 1) {
    return new Kernel::AtomicFormula(lits[0]);
  }

  Kernel::FormulaList* formLits = Kernel::FormulaList::empty();
  for (Kernel::Literal* lit : lits) {
    Kernel::FormulaList::push(new Kernel::AtomicFormula(lit), formLits);
  }
  formLits = Kernel::FormulaList::reverse(formLits);

  return Kernel::JunctionFormula::generalJunction(Kernel::OR, formLits);
}

/**
 * @brief Estrae tutti i letterali da una formula che rappresenta una disgiunzione o un letterale singolo.
 */
bool Lemma5::extractLiteralsFromFormula(Kernel::Formula* form, std::vector<Kernel::Literal*>& lits)
{
  if (!form) return false;

  if (form->connective() == Kernel::LITERAL) {
    lits.push_back(form->literal());
    return true;
  }

  if (form->connective() == Kernel::NOT && form->uarg()->connective() == Kernel::LITERAL) {
    lits.push_back(Kernel::Literal::create(form->uarg()->literal(), false));
    return true;
  }

  if (form->connective() == Kernel::OR) {
    Kernel::FormulaList::Iterator it(form->args());
    while (it.hasNext()) {
      if (!extractLiteralsFromFormula(it.next(), lits)) {
        return false;
      }
    }
    return true;
  }

  if (form->connective() == Kernel::FORALL) {
    return extractLiteralsFromFormula(form->qarg(), lits);
  }

  return false;
}

/**
 * @brief Decompone una lista di letterali separando l'uguaglianza positiva x = y dai letterali unari gamma(x) e delta(y).
 */
bool Lemma5::decomposeLiterals(const std::vector<Kernel::Literal*>& lits, std::vector<Kernel::Literal*>& gammaLits, std::vector<Kernel::Literal*>& deltaLits)
{
  bool hasPositiveEq = false;

  for (Kernel::Literal* lit : lits) {
    if (lit->isEquality() && lit->isPositive()) {
      Kernel::TermList arg0 = *lit->nthArgument(0);
      Kernel::TermList arg1 = *lit->nthArgument(1);
      if (arg0.isVar() && arg1.isVar()) {
        unsigned v0 = arg0.var();
        unsigned v1 = arg1.var();
        if ((v0 == 0 && v1 == 1) || (v0 == 1 && v1 == 0)) {
          hasPositiveEq = true;
          continue;
        }
      }
    }

    if (isUnaryOnVar(lit, 0)) {
      gammaLits.push_back(lit);
    } else if (isUnaryOnVar(lit, 1)) {
      deltaLits.push_back(renameVarYtoX(lit));
    } else {
      return false;
    }
  }

  return hasPositiveEq;
}

/**
 * @brief Decompone una clausola di Tipo 3 isolando l'uguaglianza x = y e dividendo i letterali in gamma(x) e delta(y).
 */
bool Lemma5::decomposeType3Clause(Kernel::Clause* cl, std::vector<Kernel::Literal*>& gammaLits, std::vector<Kernel::Literal*>& deltaLits)
{
  if (!cl) return false;

  std::vector<Kernel::Literal*> lits;
  lits.reserve(cl->length());
  for (unsigned i = 0; i < cl->length(); i++) {
    lits.push_back((*cl)[i]);
  }

  return decomposeLiterals(lits, gammaLits, deltaLits);
}

/**
 * @brief Applica il Lemma 5 per trasformare le formule/clausole di Tipo 3 con uguaglianza x = y nei 3 rami dell'equivalenza.
 */
void Lemma5::applyLemma5(Kernel::Problem &prb)
{
  std::cout << "\n--- INIZIO APPLICAZIONE LEMMA 5 ---\n";

  const Kernel::TermList sort = Kernel::AtomicSort::defaultSort();
  UnitList* newUnits = UnitList::empty();
  unsigned transformedCount = 0;

  UnitList::DelIterator it(prb.units());
  while (it.hasNext()) {
    Kernel::Unit* unit = it.next();
    std::vector<Kernel::Literal*> gammaLits;
    std::vector<Kernel::Literal*> deltaLits;
    bool canTransform = false;

    if (unit->isClause()) {
      Kernel::Clause* cl = static_cast<Kernel::Clause*>(unit);
      canTransform = decomposeType3Clause(cl, gammaLits, deltaLits);
    } else {
      Kernel::FormulaUnit* fu = static_cast<Kernel::FormulaUnit*>(unit);
      std::vector<Kernel::Literal*> lits;
      if (extractLiteralsFromFormula(fu->formula(), lits)) {
        canTransform = decomposeLiterals(lits, gammaLits, deltaLits);
      }
    }

    if (canTransform) {
      transformedCount++;
      std::cout << "[Lemma 5] Trasformazione unita' di Tipo 3 con uguaglianza: " << unit->toString() << "\n";

      Kernel::Formula* gammaX = createDisjunctionFromLiterals(gammaLits);
      Kernel::Formula* deltaX = createDisjunctionFromLiterals(deltaLits);

      std::vector<Kernel::Literal*> gammaLitsY;
      for (Kernel::Literal* lit : gammaLits) {
        unsigned arity = lit->arity();
        std::vector<Kernel::TermList> args;
        for (unsigned i = 0; i < arity; i++) {
          Kernel::TermList arg = *lit->nthArgument(i);
          if (arg.isVar() && arg.var() == 0) {
            args.push_back(Kernel::TermList::var(1));
          } else {
            args.push_back(arg);
          }
        }
        gammaLitsY.push_back(Kernel::Literal::create(lit->functor(), arity, lit->polarity(), args.data()));
      }
      Kernel::Formula* gammaY = createDisjunctionFromLiterals(gammaLitsY);

      Kernel::Formula* ramo1 = new Kernel::QuantifiedFormula(Kernel::FORALL, Kernel::VSList::singleton({0u, sort}), gammaX);
      Kernel::Formula* ramo2 = new Kernel::QuantifiedFormula(Kernel::FORALL, Kernel::VSList::singleton({0u, sort}), deltaX);

      Kernel::Formula* notGammaX = new Kernel::NegatedFormula(gammaX);
      Kernel::Formula* notGammaY = new Kernel::NegatedFormula(gammaY);

      Kernel::Literal* eqLit = Kernel::Literal::createEquality(true, Kernel::TermList::var(0), Kernel::TermList::var(1), sort);
      Kernel::Formula* impEq = new Kernel::BinaryFormula(Kernel::IMP, notGammaY, new Kernel::AtomicFormula(eqLit));
      Kernel::Formula* forallYImp = new Kernel::QuantifiedFormula(Kernel::FORALL, Kernel::VSList::singleton({1u, sort}), impEq);

      Kernel::FormulaList* andArgs1 = Kernel::FormulaList::empty();
      Kernel::FormulaList::push(forallYImp, andArgs1);
      Kernel::FormulaList::push(notGammaX, andArgs1);
      Kernel::Formula* existsUniqueBody = Kernel::JunctionFormula::generalJunction(Kernel::AND, andArgs1);
      Kernel::Formula* existsUnique = new Kernel::QuantifiedFormula(Kernel::EXISTS, Kernel::VSList::singleton({0u, sort}), existsUniqueBody);

      Kernel::Formula* coimp = new Kernel::QuantifiedFormula(Kernel::FORALL, Kernel::VSList::singleton({0u, sort}),
                                                             new Kernel::BinaryFormula(Kernel::IFF, gammaX, deltaX));

      Kernel::FormulaList* andArgs2 = Kernel::FormulaList::empty();
      Kernel::FormulaList::push(coimp, andArgs2);
      Kernel::FormulaList::push(existsUnique, andArgs2);
      Kernel::Formula* ramo3 = Kernel::JunctionFormula::generalJunction(Kernel::AND, andArgs2);

      Kernel::FormulaList* branches = Kernel::FormulaList::empty();
      Kernel::FormulaList::push(ramo3, branches);
      Kernel::FormulaList::push(ramo2, branches);
      Kernel::FormulaList::push(ramo1, branches);
      Kernel::Formula* equivFormula = Kernel::JunctionFormula::generalJunction(Kernel::OR, branches);

      std::cout << "[Lemma 5] Formula generata senza uguaglianza diretta: " << equivFormula->toString().substr(0, 80) << "...\n";

      Kernel::Unit* newUnit = new Kernel::FormulaUnit(equivFormula, Kernel::Inference(Kernel::FromInput(Kernel::UnitInputType::AXIOM)));
      Kernel::UnitList::push(newUnit, newUnits);
      continue;
    }

    Kernel::UnitList::push(unit, newUnits);
  }

  prb.units() = Kernel::UnitList::reverse(newUnits);

  std::cout << "[Lemma 5] Trasformazione completata. Clausole modificate: " << transformedCount << "\n";
  std::cout << "--- FINE LEMMA 5 ---\n\n";
}

} // namespace Lemmata
} // namespace FO2Fragment
