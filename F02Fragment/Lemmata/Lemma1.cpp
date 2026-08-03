#include "Lemma1.hpp"

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

#include <iostream>
#include <string>

using namespace Kernel;
using namespace Shell;

namespace FO2Fragment {
namespace {

Formula *replaceInEquality(Literal *lit, DHMap<unsigned, unsigned> &constants)
{
  std::cout << " [1 trovata uguaglianza ]" << "\n";
  ASS(lit);
  ASS(lit->isEquality());

  TermList left = *lit->nthArgument(0);
  TermList right = *lit->nthArgument(1);
  bool changed = false;

  std::cout << " [2 inizio analisi termine sinistro ]" << "\n";


  if (!left.isVar() && env.signature->getFunction(left.term()->functor())->arity() == 0) {
    std::cout << " [TROVATA COSTANTE IN UGUAGLIANZA (SX)] ---\n";
    unsigned constantFunctor = left.term()->functor();
    unsigned predicateFunctor;

    if (!constants.find(constantFunctor, predicateFunctor)) {
      std::string constName = env.signature->getFunction(constantFunctor)->name();
      std::string predName = "p_" + constName;
      predicateFunctor = env.signature->addFreshPredicate(1, predName.c_str());
      constants.insert(constantFunctor, predicateFunctor);
    }

    left = TermList::var(0);
    changed = true;
  }

  std::cout << " [3 inizio analisi termine destro ]" << "\n";


  if (!right.isVar() && env.signature->getFunction(right.term()->functor())->arity() == 0) {
    std::cout << " [TROVATA COSTANTE IN UGUAGLIANZA (DX)] ---\n";
    unsigned constantFunctor = right.term()->functor();
    unsigned predicateFunctor;

    if (!constants.find(constantFunctor, predicateFunctor)) {
      std::string constName = env.signature->getFunction(constantFunctor)->name();
      std::string predName = "p_" + constName;
      predicateFunctor = env.signature->addFreshPredicate(1, predName.c_str());
      constants.insert(constantFunctor, predicateFunctor);
    }

    right = TermList::var(1);
    changed = true;
  }


  if (changed) {
    const TermList sort = AtomicSort::defaultSort();
    Literal *modifiedEq = Literal::createEquality(lit->polarity(), left, right, sort);
    return new AtomicFormula(modifiedEq);
  }


  return nullptr;
}

Formula *replaceinAtomicFormula(Formula *formula, DHMap<unsigned, unsigned> &constants)
{

  ASS(formula);
  ASS_EQ(formula->connective(), LITERAL);

  Literal *lit = formula->literal();

  ASS(lit);

  if (!lit) {
    return formula;
  }

  if (lit->isEquality()) {
    Formula *eqResult = replaceInEquality(lit, constants);
    if (eqResult) {
      return eqResult;
    }
    return formula;
  }

  unsigned numArgs = lit->arity();


  std::vector<TermList> newArgs;
  newArgs.reserve(numArgs);

  for (unsigned i = 0; i < numArgs; ++i) {
    TermList arg = *lit->nthArgument(i);

    if (arg.isVar()) {
      newArgs.push_back(arg);
    }
    else {
      Term *t = arg.term();
      if (t->arity() == 0) {
        unsigned constantFunctor = t->functor();
        unsigned predicateFunctor;


        if (!constants.find(constantFunctor, predicateFunctor)) {
          std::cout << "[TROVATA COSTANTE ] ---\n";

          std::string constName = env.signature->getFunction(constantFunctor)->name();
          std::string predName = "p_" + constName;

          predicateFunctor = env.signature->addFreshPredicate(1, predName.c_str());

          constants.insert(constantFunctor, predicateFunctor);
        }


        unsigned freshVarIndex = i;
        TermList freshVar = TermList(freshVarIndex, false);

        newArgs.push_back(freshVar);
      }
      else {
        newArgs.push_back(arg);
      }
    }
  }

  Literal *modifiedLit = Literal::create(
      lit->functor(),
      numArgs,
      lit->polarity(),
      newArgs.data());

  AtomicFormula *nuova = new AtomicFormula(modifiedLit);
  return nuova;
}

Formula *makeUniquenessAxiom(unsigned predicateFunctor)
{
  std::cout << "\n--- GENERAZIONE ASSIOMA DI UNICITA' ---\n";

  const TermList sort = AtomicSort::defaultSort();
  TermList x = TermList::var(0);
  TermList y = TermList::var(1);


  Formula *predY = new AtomicFormula(Literal::create1(predicateFunctor, true, y));
  Formula *eq = new AtomicFormula(Literal::createEquality(true, x, y, sort));


  Formula *implication = new BinaryFormula(Connective::IMP, predY, eq);
  std::cout << "[Assioma Step 1] Implicazione (p(y) -> x=y): " << implication->toString() << "\n";


  Formula *universalY = new QuantifiedFormula(Connective::FORALL,
                                              VSList::singleton({1u, sort}),
                                              implication);
  std::cout << "[Assioma Step 2] universalY (∀y ...): " << universalY->toString() << "\n";


  Formula *predX = new AtomicFormula(Literal::create1(predicateFunctor, true, x));


  FormulaList *andArgs = FormulaList::empty();
  FormulaList::push(universalY, andArgs);
  FormulaList::push(predX, andArgs);
  Formula *conjunction = JunctionFormula::generalJunction(Connective::AND, andArgs);
  std::cout << "[Assioma Step 3] Congiunzione (p(x) & ∀y ...): " << conjunction->toString() << "\n";


  Formula *finalAxiom = new QuantifiedFormula(Connective::EXISTS,
                                              VSList::singleton({0u, sort}),
                                              conjunction);

  std::cout << "[Assioma FINALE]: " << finalAxiom->toString() << "\n";
  std::cout << "-------------------------------------------------\n\n";

  return finalAxiom;
}

Formula *replaceInFormula(Formula *formula, DHMap<unsigned, unsigned> &constants)
{
  if (!formula)
    return nullptr;

  switch (formula->connective()) {
    case LITERAL: {

      return replaceinAtomicFormula(formula, constants);
      std::cout << "[return tutto ok] ---\n";
    }
    case IMP:
    case IFF:
    case XOR: {
      Formula *oldLeft = formula->left();
      Formula *oldRight = formula->right();

      Formula *newLeft = replaceInFormula(oldLeft, constants);
      Formula *newRight = replaceInFormula(oldRight, constants);

      if (newLeft != oldLeft || newRight != oldRight) {
        return new BinaryFormula(formula->connective(), newLeft, newRight);
      }
      else {
        return formula;
      }
    }
    case AND:
    case OR: {

      FormulaList *args = formula->args();
      FormulaList *newArgs = FormulaList::empty();
      bool changed = false;

      FormulaList::Iterator it(args);
      while (it.hasNext()) {
        Formula *arg = it.next();
        Formula *newArg = replaceInFormula(arg, constants);
        if (newArg != arg)
          changed = true;
        FormulaList::push(newArg, newArgs);
      }
      newArgs = FormulaList::reverse(newArgs);

      if (changed) {
        return new JunctionFormula(formula->connective(), newArgs);
      }
      else {

        FormulaList::destroy(newArgs);
        return formula;
      }
    }
    case NOT: {
      Formula *oldArg = formula->uarg();
      Formula *newArg = replaceInFormula(oldArg, constants);

      if (newArg != oldArg) {
        return new NegatedFormula(newArg);
      }
      return formula;
    }

    case FORALL:
    case EXISTS: {
      std::cout << "[RICORSIONE] Entrato in FORALL/EXISTS\n";
      Formula *oldSub = formula->qarg();
      std::cout << "[RICORSIONE] Sto per chiamare replaceInFormula sulla sottoformula...\n";
      Formula *newSub = replaceInFormula(oldSub, constants);

      std::cout << "[RICORSIONE] Ritornato dalla sottoformula! Confronto i puntatori...\n";
      if (newSub != oldSub) {
        std::cout << "[RICORSIONE] La formula è cambiata, provo a creare QuantifiedFormula...\n";
        Formula *ris = new QuantifiedFormula(formula->connective(), formula->vars(), newSub);
        std::cout << "[RICORSIONE] QuantifiedFormula creata con successo!\n";
        return ris;
      }
      else {
        std::cout << "[RICORSIONE] La formula NON è cambiata, restituisco l'originale\n";
        return formula;
      }
    }
    default:
      return formula;
  }
}

Unit *replaceInClause(Clause *cl, DHMap<unsigned, unsigned> &constants)
{
  bool modified = false;

  Stack<Formula *> processedLiterals;

  const unsigned len = cl->length();
  for (unsigned i = 0; i < len; ++i) {
    Literal *lit = (*cl)[i];

    Formula *litFormula = new AtomicFormula(lit);

    Formula *processedFormula = replaceInFormula(litFormula, constants);

    if (processedFormula != litFormula) {
      delete litFormula;
      modified = true;
    }
    processedLiterals.push(processedFormula);
  }

  if (!modified) {
    while (!processedLiterals.isEmpty()) {
      delete processedLiterals.pop();
    }
    return cl;
  }


  FormulaList *disjuncts = FormulaList::empty();
  while (!processedLiterals.isEmpty()) {
    FormulaList::push(processedLiterals.pop(), disjuncts);
  }

  Formula *fullDisjunction = JunctionFormula::generalJunction(OR, disjuncts);

  return new FormulaUnit(fullDisjunction,
                         NonspecificInference1(InferenceRule::INPUT, cl));
}

} // namespace

void Lemma1::applyLemma1(Kernel::Problem &prb)
{

  DHMap<unsigned, unsigned> constants;


  UnitList::DelIterator it(prb.units());
  while (it.hasNext()) {
    Unit *unit = it.next();

    if (!unit->isClause()) {
      FormulaUnit *fu = static_cast<FormulaUnit *>(unit);
      Formula *originFormula = fu->formula();

      Formula *processedFormula = replaceInFormula(originFormula, constants);

      if (processedFormula != originFormula) {
        FormulaUnit *newUnit = new FormulaUnit(processedFormula, NonspecificInference1(InferenceRule::INPUT, unit));
        it.replace(newUnit);
      }
    }
    else {
      Clause *cl = static_cast<Clause *>(unit);
      Unit *processedUnit = replaceInClause(cl, constants);

      if (processedUnit != cl) {
        it.replace(processedUnit);
      }
    }
  }

  if (!constants.isEmpty()) {
    UnitList *newUnits = UnitList::empty();

    DHMap<unsigned, unsigned>::Iterator mit(constants);
    while (mit.hasNext()) {
      unsigned constantSymbolId;
      unsigned predicateSymbolId;
      mit.next(constantSymbolId, predicateSymbolId);

      Formula *uniquenessFormula = makeUniquenessAxiom(predicateSymbolId);
      Unit *uniquenessUnit = new FormulaUnit(uniquenessFormula, Inference(FromInput(UnitInputType::AXIOM)));
      UnitList::push(uniquenessUnit, newUnits);
    }
    prb.units() = UnitList::concat(newUnits, prb.units());
  }
}
} // namespace FO2Fragment