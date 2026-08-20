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
 * @brief Trasforma una clausola isolando e rimuovendo i letterali di uguaglianza.
 */
Formula *transformClauseEquality(Formula *clauseFormula, Lemma3State &state, bool &isTautology, bool &hasPositiveEq)
{
  if (!clauseFormula) {
    return nullptr;
  }

  FormulaList *literals = FormulaList::empty();
  bool createdList = false;

  if (clauseFormula->connective() == OR) {
    literals = clauseFormula->args();
  } else {
    FormulaList::push(clauseFormula, literals);
    createdList = true;
  }

  FormulaList *cleanLiterals = FormulaList::empty();
  isTautology = false;

  FormulaList::Iterator it(literals);
  while (it.hasNext()) {
    Formula *litForm = it.next();
    const Literal *lit = nullptr;

    if (litForm->connective() == LITERAL) {
      lit = litForm->literal();
    } else if (litForm->connective() == NOT && litForm->uarg()->connective() == LITERAL) {
      const Literal *baseLit = litForm->uarg()->literal();
      lit = Literal::create(const_cast<Literal*>(baseLit), false);
    }

    if (!lit) {
      FormulaList::push(litForm, cleanLiterals);
      continue;
    }

    EqualityType eqType = classifyEqualityLiteral(lit);

    switch (eqType) {
      case EqualityType::TAUTOLOGY_TRUE:
        FO2Logger::logDebug("[Lemma 3] Trovata tautologia (x = x), la clausola viene scartata.");
        isTautology = true;
        FormulaList::destroy(cleanLiterals);
        if (createdList) {
          FormulaList::destroy(literals);
        }
        return nullptr;

      case EqualityType::CONTRADICTION_FALSE:
        FO2Logger::logDebug("[Lemma 3] Rimosso letterale contraddittorio (x != x).");
        break;

      case EqualityType::POS_DISTINCT_EQ:
        FO2Logger::logDebug("[Lemma 3] Estratta uguaglianza positiva (x = y) nel prefisso implicativo.");
        hasPositiveEq = true;
        break;

      case EqualityType::NEG_DISTINCT_EQ: {
        FO2Logger::logDebug("[Lemma 3] Sostituita uguaglianza negativa (x != y) con predicato neq(x,y).");
        unsigned neqFunctor = state.getNeqPredicate();
        TermList args[2] = {*lit->nthArgument(0), *lit->nthArgument(1)};
        Literal *neqLit = Literal::create(neqFunctor, 2, true, args);
        Formula *neqFormula = new AtomicFormula(neqLit);
        FormulaList::push(neqFormula, cleanLiterals);
        break;
      }

      case EqualityType::NOT_EQUALITY:
      default:
        FormulaList::push(litForm, cleanLiterals);
        break;
    }
  }

  if (createdList) {
    FormulaList::destroy(literals);
  }

  cleanLiterals = FormulaList::reverse(cleanLiterals);
  return JunctionFormula::generalJunction(OR, cleanLiterals);
}

/**
 * @brief Processa il corpo CNF di una sottoformula pulendo le sue clausole dalle uguaglianze.
 */
Formula *processCNFBody(Formula *cnfFormula, Lemma3State &state, bool &hasPositiveEq)
{
  if (!cnfFormula) return nullptr;

  FormulaList *clauses = FormulaList::empty();
  bool createdList = false;

  if (cnfFormula->connective() == AND) {
    clauses = cnfFormula->args();
  } else {
    FormulaList::push(cnfFormula, clauses);
    createdList = true;
  }

  FormulaList *cleanClauses = FormulaList::empty();
  hasPositiveEq = false;

  FormulaList::Iterator it(clauses);
  while (it.hasNext()) {
    Formula *clause = it.next();
    bool isTautology = false;
    bool clauseHasEq = false;

    Formula *cleanClause = transformClauseEquality(clause, state, isTautology, clauseHasEq);

    if (clauseHasEq) {
      hasPositiveEq = true;
    }

    if (!isTautology && cleanClause) {
      FormulaList::push(cleanClause, cleanClauses);
    }
  }

  if (createdList) {
    FormulaList::destroy(clauses);
  }

  cleanClauses = FormulaList::reverse(cleanClauses);
  return JunctionFormula::generalJunction(AND, cleanClauses);
}

/**
 * @brief Estrae le clausole da una formula CNF pulita per il merging del Tipo 3.
 */
void extractClauses(Formula *cnfFormula, FormulaList *&targetList)
{
  if (!cnfFormula) return;

  if (cnfFormula->connective() == AND) {
    FormulaList::Iterator it(cnfFormula->args());
    while (it.hasNext()) {
      FormulaList::push(it.next(), targetList);
    }
  } else {
    FormulaList::push(cnfFormula, targetList);
  }
}

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

} // namespace

/**
 * @brief Applica il Lemma 3 al problema trasformando le uguaglianze ed isolandole.
 */
void Lemma3::applyLemma3(Kernel::Problem &prb)
{
  FO2Logger::logDebug("[Lemma 3] INIZIO APPLICAZIONE LEMMA 3");

  Lemma3State state;
  FormulaList *type3Clauses = FormulaList::empty();
  bool type3HasPositiveEq = false;

  UnitList::DelIterator it(prb.units());
  while (it.hasNext()) {
    Unit *unit = it.next();

    if (!unit->isClause()) {
      FormulaUnit *fu = static_cast<FormulaUnit *>(unit);
      Formula *formula = fu->formula();

      ScottType stype = determineScottType(formula);

      if (stype == ScottType::TYPE_3) {
        FO2Logger::logDebug("[Lemma 3] Elaborazione formula Tipo 3: " + formula->toString().substr(0, 60) + "...");
        Formula *body = formula;
        if (formula->connective() == FORALL) {
          body = formula->qarg();
          if (body && body->connective() == FORALL) {
            body = body->qarg();
          }
        }

        bool hasPosEq = false;
        Formula *cleanBody = processCNFBody(body, state, hasPosEq);
        if (hasPosEq) {
          type3HasPositiveEq = true;
        }

        extractClauses(cleanBody, type3Clauses);
        it.del();
      }
      else {
        FO2Logger::logDebug("[Lemma 3] Elaborazione formula Tipo 1/2: " + formula->toString().substr(0, 60) + "...");
        Formula *body = formula->qarg();
        bool hasPosEq = false;

        if (stype == ScottType::TYPE_2 && body && body->connective() == EXISTS) {
          Formula *innerBody = body->qarg();
          Formula *cleanInner = processCNFBody(innerBody, state, hasPosEq);
          
          Formula *newExists = new QuantifiedFormula(EXISTS, body->vars(), cleanInner);
          Formula *newForall = new QuantifiedFormula(FORALL, formula->vars(), newExists);

          FormulaUnit *newUnit = new FormulaUnit(newForall, NonspecificInference1(InferenceRule::INPUT, unit));
          it.replace(newUnit);
        }
        else {
          Formula *cleanBody = processCNFBody(body, state, hasPosEq);
          Formula *newQuant = new QuantifiedFormula(formula->connective(), formula->vars(), cleanBody);

          FormulaUnit *newUnit = new FormulaUnit(newQuant, NonspecificInference1(InferenceRule::INPUT, unit));
          it.replace(newUnit);
        }
      }
    }
    else {
      FO2Logger::logDebug("[Lemma 3] Elaborazione clausola: " + unit->toString());
      Clause *cl = static_cast<Clause *>(unit);
      FormulaList *lits = FormulaList::empty();
      for (unsigned i = 0; i < cl->length(); ++i) {
        FormulaList::push(new AtomicFormula((*cl)[i]), lits);
      }
      Formula *clauseFormula = JunctionFormula::generalJunction(OR, FormulaList::reverse(lits));
      
      bool isTautology = false;
      bool hasPosEq = false;
      Formula *cleanClause = transformClauseEquality(clauseFormula, state, isTautology, hasPosEq);

      if (hasPosEq) {
        type3HasPositiveEq = true;
      }

      if (!isTautology && cleanClause) {
        FormulaList::push(cleanClause, type3Clauses);
      }

      it.del();
    }
  }

  if (!FormulaList::isEmpty(type3Clauses)) {
    type3Clauses = FormulaList::reverse(type3Clauses);
    Formula *mergedCNF = JunctionFormula::generalJunction(AND, type3Clauses);

    const TermList sort = AtomicSort::defaultSort();
    Formula *type3Body = mergedCNF;

    if (type3HasPositiveEq) {
      FO2Logger::logDebug("[Lemma 3] Aggiunto prefisso implicativo (x != y => ...) alla formula Tipo 3 unificata.");
      Literal *eqLit = Literal::createEquality(true, TermList::var(0), TermList::var(1), sort);
      Formula *eqAtom = new AtomicFormula(eqLit);
      type3Body = new BinaryFormula(IMP, new NegatedFormula(eqAtom), mergedCNF);
    }

    Formula *forallY = new QuantifiedFormula(FORALL, VSList::singleton({1u, sort}), type3Body);
    Formula *forallX = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), forallY);

    Unit *type3Unit = new FormulaUnit(forallX, Inference(FromInput(UnitInputType::AXIOM)));
    
    UnitList *unitWrapper = UnitList::empty();
    UnitList::push(type3Unit, unitWrapper);
    prb.units() = UnitList::concat(prb.units(), unitWrapper);
    FO2Logger::logDebug("[Lemma 3] Generata formula unificata di Tipo 3: " + forallX->toString());
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
