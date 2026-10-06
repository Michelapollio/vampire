#include "Lemma4.hpp"

#include "Kernel/TermIterators.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Unit.hpp"
#include "Lib/Stack.hpp"
#include "F02Fragment/ScottTypes.hpp"
#include "F02Fragment/FO2Logger.hpp"
#include <vector>
#include <deque>
#include <unordered_set>
#include <iostream>
#include <string>

namespace FO2Fragment {
namespace Lemmata {

namespace {

/**
 * @brief Restituisce il termine con le variabili X0 e X1 scambiate (X0 <-> X1).
 */
Kernel::TermList swapVar(Kernel::TermList t)
{
  if (t.isVar()) {
    if (t.var() == 0) return Kernel::TermList::var(1);
    if (t.var() == 1) return Kernel::TermList::var(0);
  }
  return t;
}

} // namespace

/**
 * @brief Controlla se un letterale contiene sia la variabile 0 (X0) che la variabile 1 (X1).
 */
bool Lemma4::isBinaryLiteral(Kernel::Literal* lit)
{
  bool hasX = false;
  bool hasY = false;

  Kernel::VariableIterator vit(lit);
  while (vit.hasNext()) {
    Kernel::TermList tl = vit.next();
    unsigned var = tl.var();
    if (var == 0) {
      hasX = true;
    } else if (var == 1) {
      hasY = true;
    }
  }

  return hasX && hasY;
}

/**
 * @brief Controlla se due letterali binari sono opposti (con o senza scambio X0 <-> X1).
 */
bool Lemma4::areResolvable(Kernel::Literal* litA, Kernel::Literal* litB, bool& needsSwap)
{
  if (litA->functor() != litB->functor() || litA->polarity() == litB->polarity()) {
    return false;
  }

  if (*litA->nthArgument(0) == *litB->nthArgument(0) &&
      *litA->nthArgument(1) == *litB->nthArgument(1)) {
    needsSwap = false;
    return true;
  }

  if (*litA->nthArgument(0) == swapVar(*litB->nthArgument(0)) &&
      *litA->nthArgument(1) == swapVar(*litB->nthArgument(1))) {
    needsSwap = true;
    return true;
  }

  return false;
}

/**
 * @brief Restituisce un nuovo letterale scambiando le variabili X0 e X1 al suo interno.
 */
Kernel::Literal* Lemma4::swapVarsInLiteral(Kernel::Literal* lit)
{
  if (lit->isEquality()) {
    Kernel::TermList srt = Kernel::AtomicSort::defaultSort();
    if (lit->isTwoVarEquality()) {
      srt = lit->twoVarEqSort();
    } else {
      if (!Kernel::SortHelper::tryGetResultSort(*lit->nthArgument(0), srt)) {
        Kernel::SortHelper::tryGetResultSort(*lit->nthArgument(1), srt);
      }
    }
    Kernel::Literal* res = Kernel::Literal::createEquality(lit->polarity(), 
                                           swapVar(*lit->nthArgument(0)), 
                                           swapVar(*lit->nthArgument(1)), 
                                           srt);
    return res;
  }

  unsigned arity = lit->arity();
  std::vector<Kernel::TermList> args;
  args.reserve(arity);
  for (unsigned i = 0; i < arity; i++) {
    args.push_back(swapVar(*lit->nthArgument(i)));
  }

  return Kernel::Literal::create(lit->functor(), arity, lit->polarity(), args.data());
}

/**
 * @brief Esegue la risoluzione ristretta (sui letterali binari) tra due clausole.
 */
void Lemma4::resolveRestricted(Kernel::Clause* clA, Kernel::Clause* clB, std::vector<Kernel::Clause*>& newResolvents)
{
  unsigned lenA = clA->length();
  unsigned lenB = clB->length();

  for (unsigned i = 0; i < lenA; i++) {
    Kernel::Literal* litA = (*clA)[i];
    
    if (!isBinaryLiteral(litA) || litA->isEquality()) continue;

    for (unsigned j = 0; j < lenB; j++) {
      Kernel::Literal* litB = (*clB)[j];
      
      if (!isBinaryLiteral(litB) || litB->isEquality()) continue;

      bool needsSwap = false;
      if (areResolvable(litA, litB, needsSwap)) {
        
        Lib::Stack<Kernel::Literal*> resLits;

        for (unsigned k = 0; k < lenA; k++) {
          if (k != i) {
            Kernel::Literal* toAdd = (*clA)[k];
            bool isDuplicate = false;
            for (unsigned m = 0; m < resLits.size(); m++) {
              if (resLits[m] == toAdd) { isDuplicate = true; break; }
            }
            if (!isDuplicate) resLits.push(toAdd);
          }
        }

        for (unsigned k = 0; k < lenB; k++) {
          if (k != j) {
            Kernel::Literal* toAdd = (*clB)[k];
            if (needsSwap) {
              toAdd = swapVarsInLiteral(toAdd);
            }
            
            bool isDuplicate = false;
            for (unsigned m = 0; m < resLits.size(); m++) {
              if (resLits[m] == toAdd) {
                isDuplicate = true;
                break;
              }
            }
            if (!isDuplicate) {
              resLits.push(toAdd);
            }
          }
        }

        bool isTautology = false;
        for (unsigned k = 0; k < resLits.size(); k++) {
          for (unsigned m = k + 1; m < resLits.size(); m++) {
            if (Kernel::Literal::complementaryLiteral(resLits[k]) == resLits[m]) {
              isTautology = true;
              break;
            }
          }
          if (isTautology) break;
        }

        if (isTautology) {
          continue;
        }

        Kernel::Inference inf(Kernel::NonspecificInference2(Kernel::InferenceRule::RESOLUTION, clA, clB));
        Kernel::Clause* resolvent = Kernel::Clause::fromStack(resLits, inf);

//        if (FO2Logger::showsDebug()) {
//          FO2Logger::logDebug("[Lemma 4] Generato risolvente ristretto" + std::string(needsSwap ? " (con scambio di variabili)" : "") + ": " + resolvent->toNiceString());
//        }
        newResolvents.push_back(resolvent);
      }
    }
  }
}

static Kernel::Literal* ensureValidTwoVarEquality(Kernel::Literal* lit) {
  if (lit && lit->isEquality() && lit->nthArgument(0)->isVar() && lit->nthArgument(1)->isVar() && !lit->isTwoVarEquality()) {
    return Kernel::Literal::createEquality(lit->polarity(), *lit->nthArgument(0), *lit->nthArgument(1), Kernel::AtomicSort::defaultSort());
  }
  return lit;
}

/**
 * @brief Converte una formula atomica o negata in un Literal, nullptr altrimenti.
 */
Kernel::Literal* Lemma4::formulaToLiteral(Kernel::Formula* formula)
{
  if (!formula) return nullptr;

  if (formula->connective() == LITERAL) {
    return ensureValidTwoVarEquality(formula->literal());
  }

  if (formula->connective() == NOT && formula->uarg()->connective() == LITERAL) {
    Kernel::Literal* inner = formula->uarg()->literal();
    return ensureValidTwoVarEquality(Kernel::Literal::create(inner, !inner->polarity()));
  }

  return nullptr;
}

/**
 * @brief Rimuove i quantificatori universali esterni ∀x∀y da una formula di Tipo 3.
 */
Kernel::Formula* Lemma4::stripForallXY(Kernel::Formula* formula)
{
  while (formula && formula->connective() == FORALL) {
    formula = formula->qarg();
  }
  return formula;
}

/**
 * @brief Estrae le clausole da una formula matrice (corpo di ∀x∀y).
 */
void Lemma4::extractClausesFromBody(Kernel::Formula* body, Kernel::Unit* parent, std::vector<Kernel::Clause*>& out)
{
  if (!body) return;

  switch (body->connective()) {
    case LITERAL:
    case NOT: {
      Kernel::Literal* lit = formulaToLiteral(body);
      if (lit) {
        Lib::Stack<Kernel::Literal*> lits;
        lits.push(lit);
        Kernel::Inference inf(NonspecificInference1(InferenceRule::CLAUSIFY, parent));
        out.push_back(Kernel::Clause::fromStack(lits, inf));
      }
      break;
    }

    case OR: {
      Lib::Stack<Kernel::Literal*> lits;
      bool allLiterals = true;
      FormulaList::Iterator it(body->args());
      while (it.hasNext()) {
        Kernel::Literal* lit = formulaToLiteral(it.next());
        if (!lit) { allLiterals = false; break; }
        lits.push(lit);
      }
      if (allLiterals) {
        Kernel::Inference inf(NonspecificInference1(InferenceRule::CLAUSIFY, parent));
        out.push_back(Kernel::Clause::fromStack(lits, inf));
      }
      break;
    }

    case AND: {
      FormulaList::Iterator it(body->args());
      while (it.hasNext()) {
        extractClausesFromBody(it.next(), parent, out);
      }
      break;
    }

    case IFF: {
      Kernel::Formula* left = body->left();
      Kernel::Formula* right = body->right();
      Kernel::Formula* imp1 = new Kernel::BinaryFormula(IMP, left, right);
      Kernel::Formula* imp2 = new Kernel::BinaryFormula(IMP, right, left);
      extractClausesFromBody(imp1, parent, out);
      extractClausesFromBody(imp2, parent, out);
      break;
    }

    case IMP: {
      Kernel::Formula* left = body->left();
      Kernel::Formula* right = body->right();
      if (right->connective() == AND) {
        Kernel::FormulaList::Iterator it(right->args());
        while (it.hasNext()) {
          Kernel::Formula* impSingle = new Kernel::BinaryFormula(IMP, left, it.next());
          extractClausesFromBody(impSingle, parent, out);
        }
      } else if (left->connective() == AND) {
        Lib::Stack<Kernel::Literal*> lits;
        bool ok = true;
        Kernel::FormulaList::Iterator it(left->args());
        while (it.hasNext()) {
          Kernel::Literal* l = formulaToLiteral(it.next());
          if (!l) { ok = false; break; }
          lits.push(Kernel::Literal::create(l, !l->polarity()));
        }
        if (ok) {
          if (right->connective() == LITERAL || (right->connective() == NOT && right->uarg()->connective() == LITERAL)) {
            Kernel::Literal* rlit = formulaToLiteral(right);
            if (rlit) lits.push(rlit);
            else ok = false;
          } else if (right->connective() == OR) {
            Kernel::FormulaList::Iterator rit(right->args());
            while (rit.hasNext()) {
              Kernel::Literal* rlit = formulaToLiteral(rit.next());
              if (!rlit) { ok = false; break; }
              lits.push(rlit);
            }
          } else {
            ok = false;
          }
        }
        if (ok) {
          Kernel::Inference inf(NonspecificInference1(InferenceRule::CLAUSIFY, parent));
          out.push_back(Kernel::Clause::fromStack(lits, inf));
        }
      } else {
        Lib::Stack<Kernel::Literal*> lits;
        Kernel::Literal* lLit = formulaToLiteral(left);
        if (lLit) {
          lits.push(Kernel::Literal::create(lLit, !lLit->polarity()));
          bool okRight = true;
          if (right->connective() == LITERAL || (right->connective() == NOT && right->uarg()->connective() == LITERAL)) {
            Kernel::Literal* rlit = formulaToLiteral(right);
            if (rlit) lits.push(rlit);
            else okRight = false;
          } else if (right->connective() == OR) {
            Kernel::FormulaList::Iterator rit(right->args());
            while (rit.hasNext()) {
              Kernel::Literal* rlit = formulaToLiteral(rit.next());
              if (!rlit) { okRight = false; break; }
              lits.push(rlit);
            }
          } else {
            okRight = false;
          }
          if (okRight) {
            Kernel::Inference inf(NonspecificInference1(InferenceRule::CLAUSIFY, parent));
            out.push_back(Kernel::Clause::fromStack(lits, inf));
          }
        }
      }
      break;
    }

    default:
      break;
  }
}

/**
 * @brief Dato un FormulaUnit di Tipo 3 (∀x∀y. corpo), estrae le clausole del corpo.
 */
void Lemma4::extractType3Clauses(Kernel::FormulaUnit* fu, std::vector<Kernel::Clause*>& out)
{
  Kernel::Formula* matrix = stripForallXY(fu->formula());
  if (!matrix) return;

  extractClausesFromBody(matrix, fu, out);
}

static std::string getClauseKey(Kernel::Clause* cl) {
  if (!cl) return "";
  std::vector<std::string> lits;
  lits.reserve(cl->length());
  for (unsigned i = 0; i < cl->length(); i++) {
    lits.push_back((*cl)[i]->toString());
  }
  std::sort(lits.begin(), lits.end());
  std::string res;
  for (const auto& s : lits) {
    res += s + " | ";
  }
  return res;
}

/**
 * @brief Lemma 4: saturazione parziale per risoluzione ristretta.
 */
void Lemma4::applyLemma4(Kernel::Problem &prb)
{
  FO2Logger::logDebug("[Lemma 4] INIZIO APPLICAZIONE LEMMA 4");

  std::vector<Kernel::Clause*> clauses2;
  std::deque<Kernel::Clause*> passive3;
  std::vector<Kernel::Clause*> active3;
  std::vector<Kernel::Unit*> nonClauseUnits;

  UnitList::DelIterator it(prb.units());
  while (it.hasNext()) {
    Kernel::Unit* unit = it.next();
    if (unit->isClause()) {
      Kernel::Clause* cl = static_cast<Kernel::Clause*>(unit);

      bool hasBinaryNonEq = false;
      bool hasEq = false;
      for (unsigned i = 0; i < cl->length(); i++) {
        Kernel::Literal* lit = (*cl)[i];
        if (lit->isEquality()) hasEq = true;
        else if (isBinaryLiteral(lit)) hasBinaryNonEq = true;
      }

      if (hasBinaryNonEq || hasEq) {
        passive3.push_back(cl);
      } else {
        clauses2.push_back(cl);
      }
    } else {
      Kernel::FormulaUnit* fu = static_cast<Kernel::FormulaUnit*>(unit);
      nonClauseUnits.push_back(unit);

      std::vector<Kernel::Clause*> extracted;
      extractType3Clauses(fu, extracted);

      if (!extracted.empty()) {
        for (Kernel::Clause* cl : extracted) {
          bool hasBinaryNonEq = false;
          bool hasEq = false;
          for (unsigned i = 0; i < cl->length(); i++) {
            Kernel::Literal* lit = (*cl)[i];
            if (lit->isEquality()) hasEq = true;
            else if (isBinaryLiteral(lit)) hasBinaryNonEq = true;
          }

          if (hasBinaryNonEq || hasEq) {
            passive3.push_back(cl);
          } else {
            clauses2.push_back(cl);
          }
        }
      }
    }
  }

  FO2Logger::logDebug("[Lemma 4] Clausole Tipo 2: " + std::to_string(clauses2.size()) + ", Clausole Tipo 3 (passive): " + std::to_string(passive3.size()));

  std::unordered_set<std::string> knownKeys;
  for (auto* cl : passive3) knownKeys.insert(getClauseKey(cl));
  for (auto* cl : clauses2)  knownKeys.insert(getClauseKey(cl));

  while (!passive3.empty()) {
    Kernel::Clause* given = passive3.front();
    passive3.pop_front();

    std::vector<Kernel::Clause*> newResolvents;

    for (Kernel::Clause* active : active3) {
      resolveRestricted(given, active, newResolvents);
    }

    std::vector<Kernel::Clause*> newType2Resolvents;
    for (Kernel::Clause* cl2 : clauses2) {
      resolveRestricted(given, cl2, newType2Resolvents);
    }
    for (Kernel::Clause* res : newType2Resolvents) {
      std::string key = getClauseKey(res);
      if (knownKeys.find(key) == knownKeys.end()) {
        knownKeys.insert(key);
        clauses2.push_back(res);
      }
    }

    active3.push_back(given);

    for (Kernel::Clause* res : newResolvents) {
      std::string key = getClauseKey(res);
      if (knownKeys.find(key) == knownKeys.end()) {
        knownKeys.insert(key);
        passive3.push_back(res);
      }
    }
  }

  FO2Logger::logDebug("[Lemma 4] Saturazione completata. Clausole Tipo 3 sature: " + std::to_string(active3.size()));

  std::vector<Kernel::Clause*> phi4 = active3;

  FO2Logger::logDebug("[Lemma 4] (phi_4): " + std::to_string(phi4.size()) + " clausole Tipo 3");

  UnitList* newUnits = UnitList::empty();

  for (Kernel::Clause* cl : phi4) {
    UnitList::push(cl, newUnits);
  }
  for (Kernel::Clause* cl : clauses2) {
    UnitList::push(cl, newUnits);
  }
  for (Kernel::Unit* u : nonClauseUnits) {
    UnitList::push(u, newUnits);
  }

  prb.units() = newUnits;

  FO2Logger::logDebug("[Lemma 4] FINE LEMMA 4");
}

} // namespace Lemmata
} // namespace FO2Fragment
