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

#include <iostream>
#include <string>

using namespace Kernel;
using namespace Shell;

namespace FO2Fragment {
namespace {

Formula *replaceinAtomicFormula(Formula *formula, DHMap<unsigned, unsigned> &constants)
{
  ASS(formula);
  ASS_EQ(formula->connective(), LITERAL);

  Literal *lit = formula->literal();

  ASS(lit);

  if (!lit) {
    return formula;
  }

  unsigned numArgs = lit->arity();

  // Contenitore per i nuovi argomenti modificati
  std::vector<TermList> newArgs;
  newArgs.reserve(numArgs);

  for (unsigned i = 0; i < numArgs; ++i) {
    TermList arg = *lit->nthArgument(i);

    if (arg.isVar()) {
      newArgs.push_back(arg); // Rimane invariata se è una variabile
    }
    else {
      Term *t = arg.term();
      if (t->arity() == 0) { // È una costante!
        unsigned constantFunctor = t->functor();
        unsigned predicateFunctor;

        // Controllo se la costante è presente nella mappa di sostituzione
        if (!constants.find(constantFunctor, predicateFunctor)) {
          std::cout << "[TROVATA COSTANTE ] ---\n";

          std::string constName = env.signature->getFunction(constantFunctor)->name();
          std::string predName = "p_" + constName;
          std::cout << "[1] ---\n";
          predicateFunctor = env.signature->addFreshPredicate(1, predName.c_str());

          constants.insert(constantFunctor, predicateFunctor);
          std::cout << "[2] ---\n";
        }

        // Creiamo la variabile fresca per sostituire la costante
        unsigned freshVarIndex = i;
        TermList freshVar = TermList(freshVarIndex, false);
        std::cout << "[3] ---\n";

        newArgs.push_back(freshVar);
        std::cout << "[4] ---\n";
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
  std::cout << "[5] ---\n";

  AtomicFormula *nuova = new AtomicFormula(modifiedLit);
  // return new AtomicFormula(modifiedLit);
  std::cout << "[6] ---\n";
  return nuova;
}

Formula *makeUniquenessAxiom(unsigned predicateFunctor)
{
  std::cout << "\n--- GENERAZIONE ASSIOMA DI UNICITA' ---\n";

  const TermList sort = AtomicSort::defaultSort();
  TermList x = TermList::var(0);
  TermList y = TermList::var(1);

  // 1. Creazione delle foglie per l'implicazione
  Formula *predY = new AtomicFormula(Literal::create1(predicateFunctor, true, y));
  Formula *eq = new AtomicFormula(Literal::createEquality(true, x, y, sort));

  // 2. Implicazione: p(y) -> x = y (Usiamo BinaryFormula, corretta per IMP)
  Formula *implication = new BinaryFormula(Connective::IMP, predY, eq);
  std::cout << "[Assioma Step 1] Implicazione (p(y) -> x=y): " << implication->toString() << "\n";

  // 3. Quantificazione universale: ! [y] : (p(y) -> x = y)
  Formula *universalY = new QuantifiedFormula(Connective::FORALL,
                                              VSList::singleton({1u, sort}),
                                              implication);
  std::cout << "[Assioma Step 2] universalY (∀y ...): " << universalY->toString() << "\n";

  // 4. Foglia per la congiunzione: p(x)
  Formula *predX = new AtomicFormula(Literal::create1(predicateFunctor, true, x));

  // 5. Congiunzione: p(x) & ! [y] : (p(y) -> x = y) (Usiamo JunctionFormula, corretta per AND)
  FormulaList *andArgs = FormulaList::empty();
  FormulaList::push(universalY, andArgs);
  FormulaList::push(predX, andArgs);
  Formula *conjunction = JunctionFormula::generalJunction(Connective::AND, andArgs);
  std::cout << "[Assioma Step 3] Congiunzione (p(x) & ∀y ...): " << conjunction->toString() << "\n";

  // 6. Quantificazione esistenziale: ? [x] : (p(x) & ! [y] : (p(y) -> x = y))
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
      // Caso base: formula atomica, sostituisco i termini
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
      // Itera sugli argomenti della giunzione e sostituisce ricorsivamente.
      // Ricostruisce la lista solo se almeno un argomento è cambiato.
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
        // Nessuna modifica: distruggi la lista temporanea e restituisci l'originale.
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

  // Caso 2: almeno un letterale modificato → costruiamo la disgiunzione.
  FormulaList *disjuncts = FormulaList::empty();
  while (!processedLiterals.isEmpty()) {
    FormulaList::push(processedLiterals.pop(), disjuncts);
  }
  
  Formula *fullDisjunction = JunctionFormula::generalJunction(OR, disjuncts);

  return new FormulaUnit(fullDisjunction,
                         NonspecificInference1(InferenceRule::INPUT, cl));
}

} // namespace

void RemoveEquality::applyLemma1(Kernel::Problem &prb)
{

  DHMap<unsigned, unsigned> constants;
  // UnitList *introducedUnits = 0;

  UnitList::DelIterator it(prb.units());
  while (it.hasNext()) {
    Unit *unit = it.next();

    if (!unit->isClause()) {
      FormulaUnit *fu = static_cast<FormulaUnit *>(unit);
      Formula *originFormula = fu->formula();

      Formula *processedFormula = replaceInFormula(originFormula, constants);
      std::cout << "[sono tornata qui ] ...\n";

      if (processedFormula != originFormula) {
        FormulaUnit *newUnit = new FormulaUnit(processedFormula, NonspecificInference1(InferenceRule::INPUT, unit));
        it.replace(newUnit);
      }
      std::cout << "[7] ...\n";
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
    std::cout << "[8] ...\n";
    DHMap<unsigned, unsigned>::Iterator mit(constants);
    while (mit.hasNext()) {
      unsigned constantSymbolId;
      unsigned predicateSymbolId;
      mit.next(constantSymbolId, predicateSymbolId);

      Formula *uniquenessFormula = makeUniquenessAxiom(predicateSymbolId);
      std::cout << "[9] ...\n";
      Unit *uniquenessUnit = new FormulaUnit(uniquenessFormula, Inference(InferenceRule::INPUT));
      std::cout << "[10] ...\n";
      UnitList::push(uniquenessUnit, newUnits);
      std::cout << "[11] ...\n";
    }
    prb.units() = UnitList::concat(newUnits, prb.units());
    std::cout << "[12] ...\n";
  }
}

void RemoveEquality::applyLemma2(Kernel::Problem &prb)
{
  (void)prb;
}

void RemoveEquality::applyLemma3(Kernel::Problem &prb)
{
  (void)prb;
}

void RemoveEquality::applyLemma4(Kernel::Problem &prb)
{
  (void)prb;
}

void RemoveEquality::applyLemma5(Kernel::Problem &prb)
{
  (void)prb;
}

void RemoveEquality::applyLemma6(Kernel::Problem &prb)
{
  (void)prb;
}

void RemoveEquality::removeEquality(Kernel::Problem &prb)
{
  if (!prb.hasEquality()) {
    return;
  }

  applyLemma1(prb);
  applyLemma2(prb);
  applyLemma3(prb);
  applyLemma4(prb);
  applyLemma5(prb);
  applyLemma6(prb);

  if (!prb.hasEquality()) {
    return;
  }
}

// ============================================================
//  Funzioni diagnostiche di tracing
// ============================================================

/**
 * Stampa una rappresentazione strutturata (ad albero) di tutti i termini
 * di un singolo letterale, distinguendo variabili e costanti (arità 0).
 *
 * @param lit   Il letterale da analizzare.
 * @param indent Prefisso di indentazione già calcolato dal chiamante.
 */
static void traceTerms(const Literal *lit, const std::string &indent)
{
  const unsigned ar = lit->arity();
  for (unsigned i = 0; i < ar; ++i) {
    const TermList *arg = lit->nthArgument(i);
    if (arg->isVar()) {
      std::cout << indent << "  arg[" << i << "]: VARIABILE  (id=" << arg->var() << ")\n";
    }
    else {
      // Un termine ground senza argomenti è una costante
      const Term *t = arg->term();
      if (t->arity() == 0) {
        std::cout << indent << "  arg[" << i << "]: COSTANTE   (functor=" << t->functor()
                  << ", nome=" << env.signature->functionName(t->functor()) << ")\n";
      }
      else {
        std::cout << indent << "  arg[" << i << "]: TERMINE    " << t->toString() << "\n";
      }
    }
  }
}

/**
 * Visita ricorsiva dell'albero sintattico di una formula.
 * Stampa ogni nodo con un'indentazione proporzionale alla profondità.
 *
 * @param formula  La formula da ispezionare (non viene modificata).
 * @param depth    Profondità corrente nell'albero (0 = radice).
 */
void RemoveEquality::traceFormula(Formula *formula, int depth)
{
  if (!formula) {
    std::cout << std::string(depth * 2, ' ') << "[formula nulla]\n";
    return;
  }

  // Costruisce il prefisso di indentazione una volta sola per questo livello.
  const std::string indent(depth * 2, ' ');

  switch (formula->connective()) {

    // ----------------------------------------------------------
    // Caso base: atomo (letterale)
    // ----------------------------------------------------------
    case LITERAL: {
      const Literal *lit = formula->literal();
      std::cout << indent << "FOGLIA (Atomo): " << formula->toString() << "\n";
      // Analizza ogni argomento del letterale distinguendo var/costante/termine
      traceTerms(lit, indent);
      break;
    }

    // ----------------------------------------------------------
    // Connettivi binari: IMP, IFF, XOR
    // (AND e OR hanno n-ary args via JunctionFormula, trattati dopo)
    // ----------------------------------------------------------
    case IMP:
      std::cout << indent << "CONNETTIVO BINARIO: IMP (==>)\n";
      traceFormula(formula->left(), depth + 1);
      traceFormula(formula->right(), depth + 1);
      break;

    case IFF:
      std::cout << indent << "CONNETTIVO BINARIO: IFF (<=>)\n";
      traceFormula(formula->left(), depth + 1);
      traceFormula(formula->right(), depth + 1);
      break;

    case XOR:
      std::cout << indent << "CONNETTIVO BINARIO: XOR\n";
      traceFormula(formula->left(), depth + 1);
      traceFormula(formula->right(), depth + 1);
      break;

    // ----------------------------------------------------------
    // Giunzioni n-arie: AND, OR
    // ----------------------------------------------------------
    case AND:
      std::cout << indent << "CONNETTIVO N-ARIO: AND\n";
      {
        FormulaList::Iterator it(formula->args());
        int idx = 0;
        while (it.hasNext()) {
          std::cout << indent << "  ramo[" << idx++ << "]:\n";
          traceFormula(it.next(), depth + 1);
        }
      }
      break;

    case OR:
      std::cout << indent << "CONNETTIVO N-ARIO: OR\n";
      {
        FormulaList::Iterator it(formula->args());
        int idx = 0;
        while (it.hasNext()) {
          std::cout << indent << "  ramo[" << idx++ << "]:\n";
          traceFormula(it.next(), depth + 1);
        }
      }
      break;

    // ----------------------------------------------------------
    // Negazione
    // ----------------------------------------------------------
    case NOT:
      std::cout << indent << "CONNETTIVO UNARIO: NOT\n";
      traceFormula(formula->uarg(), depth + 1);
      break;

    // ----------------------------------------------------------
    // Quantificatori
    // ----------------------------------------------------------
    case FORALL:
    case EXISTS: {
      const char *qname = (formula->connective() == FORALL) ? "FORALL" : "EXISTS";
      std::cout << indent << "QUANTIFICATORE: " << qname << " sulle variabili [";
      // VSList e' List<pair<unsigned,TermList>>: il primo elemento della coppia e' l'ID var
      const VSList *vs = formula->vars();
      bool first = true;
      VSList::Iterator vit(vs);
      while (vit.hasNext()) {
        const VarSort &vs_elem = vit.next();
        if (!first)
          std::cout << ", ";
        std::cout << "x" << vs_elem.first;
        first = false;
      }
      std::cout << "]\n";
      // Visita il corpo del quantificatore
      traceFormula(formula->qarg(), depth + 1);
      break;
    }

    default:
      std::cout << indent << "NODO SCONOSCIUTO (connective=" << formula->connective()
                << "): " << formula->toString() << "\n";
      break;
  }
}

/**
 * Stampa la struttura piatta di una clausola, ispezionando ogni letterale
 * e i suoi argomenti (variabili vs. costanti vs. termini composti).
 *
 * @param cl  La clausola da ispezionare (non viene modificata).
 */
void RemoveEquality::traceClause(Clause *cl)
{
  const unsigned len = cl->length();
  std::cout << "CLAUSOLA PIATTA: composta da " << len << " letterali\n";

  for (unsigned i = 0; i < len; ++i) {
    const Literal *lit = (*cl)[i];
    // Stampa la rappresentazione testuale del letterale
    std::cout << "  Letterale[" << i << "]: " << lit->toString() << "\n";
    // Analizza argomento per argomento
    traceTerms(lit, "  ");
  }
}

/**
 * Punto di ingresso diagnostico: itera su tutte le unit del problema
 * e delega la stampa strutturata a traceFormula o traceClause.
 *
 * @param prb  Il problema da ispezionare (accesso in sola lettura).
 */
void RemoveEquality::traceProblem(Kernel::Problem &prb)
{
  std::cout << "\n";
  std::cout << "================================================================\n";
  std::cout << "  INIZIO TRACING DEL PROBLEMA ("
            << UnitList::length(prb.units()) << " unita')\n";
  std::cout << "================================================================\n";

  int unitIndex = 0;
  UnitList::Iterator it(prb.units());
  while (it.hasNext()) {
    Unit *unit = it.next();
    std::cout << "\n--- Unita' #" << unitIndex++ << " ";

    if (!unit->isClause()) {
      // ---- Formula Unit ----
      std::cout << "[FORMULA UNIT] ---\n";
      FormulaUnit *fu = static_cast<FormulaUnit *>(unit);
      traceFormula(fu->formula(), /*depth=*/0);
    }
    else {
      // ---- Clausola ----
      std::cout << "[CLAUSOLA] ---\n";
      Clause *cl = static_cast<Clause *>(unit);
      traceClause(cl);
    }
  }

  std::cout << "\n================================================================\n";
  std::cout << "  FINE TRACING\n";
  std::cout << "================================================================\n\n";
}

} // namespace FO2Fragment
