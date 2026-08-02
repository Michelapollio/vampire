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

#include <iostream>

using namespace Kernel;

namespace FO2Fragment {
namespace {

/**
 * Classificazione di un letterale di uguaglianza per il Lemma 3.
 */
enum class EqualityType {
  NOT_EQUALITY,        // Non e' un letterale di uguaglianza
  TAUTOLOGY_TRUE,      // x = x (sempre vero -> la clausola diventa tautologia)
  CONTRADICTION_FALSE, // x != x (sempre falso -> il letterale puo' essere rimosso)
  POS_DISTINCT_EQ,     // x = y con x != y (uguaglianza positiva estratta nel prefisso)
  NEG_DISTINCT_EQ      // x != y con x != y (uguaglianza negativa sostituita con neq(x,y))
};


/**
 * 1 Analizza e classifica un letterale rispetto all'uguaglianza.
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

    // Caso 1: Stessa variabile (x = x oppure x != x)
    if (var0 == var1) {
      return lit->isPositive() ? EqualityType::TAUTOLOGY_TRUE 
                               : EqualityType::CONTRADICTION_FALSE;
    }

    // Caso 2: Variabili distinte (x = y oppure x != y)
    return lit->isPositive() ? EqualityType::POS_DISTINCT_EQ 
                             : EqualityType::NEG_DISTINCT_EQ;
  }

  return EqualityType::NOT_EQUALITY;
}

/**
 * Stato globale del Lemma 3.
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
 * Funzione 2: Trasforma una clausola isolando/rimuovendo le uguaglianze.
 * 
 * @param clauseFormula   Formula rappresentante la clausola (AtomicFormula, NOT o JunctionFormula(OR, ...)).
 * @param state           Stato globale per il tracciamento del predicato neq.
 * @param isTautology     Output: impostato a true se la clausola contiene x = x.
 * @param hasPositiveEq   Output: impostato a true se la clausola contiene x = y (var distinte).
 * @return                Formula della clausola pulita (senza uguaglianze) o nullptr se tautologia.
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
        isTautology = true;
        FormulaList::destroy(cleanLiterals);
        if (createdList) {
          FormulaList::destroy(literals);
        }
        return nullptr;

      case EqualityType::CONTRADICTION_FALSE:
        break;

      case EqualityType::POS_DISTINCT_EQ:
        hasPositiveEq = true;
        break;

      case EqualityType::NEG_DISTINCT_EQ: {
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
 * Funzione 3: Processa il corpo CNF di una sottoformula (alpha_i) pulendo tutte le clausole dalle uguaglianze.
 * 
 * @param cnfFormula      Il corpo CNF della formula.
 * @param state           Stato globale.
 * @param hasPositiveEq   Output: segnala se almeno una clausola conteneva x = y.
 * @return                Il nuovo corpo CNF pulito senza uguaglianze.
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
 * Estrae le clausole da una formula CNF pulita per il merging del Tipo 3.
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
 * Costruisce l'assioma di anti-riflessivita' per neq: ∀x (¬neq(x, x))
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
 * Funzione Principale: Applica il Lemma 3 al problema fornito.
 */
void Lemma3::applyLemma3(Kernel::Problem &prb)
{
  std::cout << "\n--- INIZIO APPLICAZIONE LEMMA 3 ---\n";

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
        // Raccogliamo tutte le formule di Tipo 3 per il merging finale
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
        it.del(); // Rimuoviamo l'unita' individuale di Tipo 3 (verra' accorpata)
      }
      else {
        // Tipo 1 (Ex) oppure Tipo 2 (Ax Ey)
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
        else { // Tipo 1
          Formula *cleanBody = processCNFBody(body, state, hasPosEq);
          Formula *newQuant = new QuantifiedFormula(formula->connective(), formula->vars(), cleanBody);

          FormulaUnit *newUnit = new FormulaUnit(newQuant, NonspecificInference1(InferenceRule::INPUT, unit));
          it.replace(newUnit);
        }
      }
    }
    else {
      // Se e' una clausola piatta (trattata come Tipo 3 implicito Ax Ay)
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

      it.del(); // Le clausole piatte vengono accorpate nella formula unica di Tipo 3
    }
  }

  // PASSAGGIO 2: Merging di tutte le unità di Tipo 3 in un'UNICA formula di Tipo 3
  if (!FormulaList::isEmpty(type3Clauses)) {
    type3Clauses = FormulaList::reverse(type3Clauses);
    Formula *mergedCNF = JunctionFormula::generalJunction(AND, type3Clauses);

    const TermList sort = AtomicSort::defaultSort();
    Formula *type3Body = mergedCNF;

    // Se almeno una clausola conteneva x = y, aggiungiamo x = y in disgiunzione nel corpo
    if (type3HasPositiveEq) {
      Literal *eqLit = Literal::createEquality(true, TermList::var(0), TermList::var(1), sort);
      Formula *eqAtom = new AtomicFormula(eqLit);
      type3Body = new BinaryFormula(IMP, new NegatedFormula(eqAtom), mergedCNF); // (x=y v mergedCNF) => not(x=y) -> mergedCNF
    }

    // Prefisso: ∀x ∀y (type3Body)
    Formula *forallY = new QuantifiedFormula(FORALL, VSList::singleton({1u, sort}), type3Body);
    Formula *forallX = new QuantifiedFormula(FORALL, VSList::singleton({0u, sort}), forallY);

    Unit *type3Unit = new FormulaUnit(forallX, Inference(InferenceRule::INPUT));
    
    UnitList *unitWrapper = UnitList::empty();
    UnitList::push(type3Unit, unitWrapper);
    prb.units() = UnitList::concat(prb.units(), unitWrapper);
  }

  // PASSAGGIO 3: Iniezione dell'assioma di anti-riflessivita' ∀x (¬neq(x,x)) se neq e' stato usato
  if (state.requiresNeqAxiom) {
    Formula *axiomFormula = createNeqReflexivityAxiom(state.getNeqPredicate());
    Unit *axiomUnit = new FormulaUnit(axiomFormula, Inference(InferenceRule::INPUT));

    UnitList *axiomWrapper = UnitList::empty();
    UnitList::push(axiomUnit, axiomWrapper);
    prb.units() = UnitList::concat(axiomWrapper, prb.units());

    std::cout << "[Lemma 3] Iniettato assioma per neq: " << axiomFormula->toString() << "\n";
  }

  std::cout << "--- FINE LEMMA 3 ---\n\n";
}

} // namespace FO2Fragment
