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
  std::cout << "--- PRIMA DEL LEMMA 1 ---" << std::endl;
  for (UnitList::Iterator it(prb.units()); it.hasNext();) {
    std::cout << it.next()->toString() << std::endl;
  }
  applyLemma1(prb);
  std::cout << "--- DOPO IL LEMMA 1 ---" << std::endl;
  for (UnitList::Iterator it(prb.units()); it.hasNext();) {
      std::cout << it.next()->toString() << std::endl;
  }
  applyLemma2(prb);
  applyLemma3(prb);
  applyLemma4(prb);
  applyLemma5(prb);
  applyLemma6(prb);

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
