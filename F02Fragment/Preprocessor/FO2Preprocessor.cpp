#include "FO2Preprocessor.hpp"

#include "Shell/Flattening.hpp" 
#include "Shell/NNF.hpp"
#include "Shell/Skolem.hpp"
#include "Shell/CNF.hpp"
#include "Shell/NewCNF.hpp"
#include "Shell/Rectify.hpp"
#include "Shell/Naming.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Term.hpp"
#include "FMB/ClauseFlattening.hpp"
#include "Lib/DHSet.hpp"
#include "Lib/Stack.hpp"
#include "Kernel/TermIterators.hpp"
#include "F02Fragment/RemoveEquality/RemoveEquality.hpp"
#include "F02Fragment/FO2Logger.hpp"

#include "Shell/SimplifyFalseTrue.hpp"
#include <map>
#include <vector>
#include <algorithm>

#include "Kernel/Signature.hpp"

namespace FO2Preprocessor {
using namespace Kernel;
using namespace Lib;
using FO2Fragment::FO2Logger;

static void normalizeClauseForS2(Clause* cl, Stack<Clause*>& outClauses)
{
  // std::cout << "[DEBUG] normalizeClauseForS2: " << cl->toString() << std::endl;
  DHSet<unsigned> clauseVars;
  VirtualIterator<unsigned> vit = cl->getVariableIterator();
  while (vit.hasNext()) {
    clauseVars.insert(vit.next());
  }

  // Check 1: 2-variable clause without a 2-variable literal (Condition 4 of Def 6)
  if (clauseVars.size() == 2) {
    bool hasTwoVarLiteral = false;
    for (int i = 0; i < (int)cl->length(); ++i) {
      Literal* lit = (*cl)[i];
      DHSet<unsigned> litVars;
      VariableIterator lvit(lit);
      while (lvit.hasNext()) {
        litVars.insert(lvit.next().var());
      }
      if (litVars.size() == 2) {
        hasTwoVarLiteral = true;
        break;
      }
    }

    if (!hasTwoVarLiteral) {
      unsigned v2 = 0;
      DHSet<unsigned>::Iterator varIt(clauseVars);
      
      // Properly extract the second variable using hasNext()
      if (varIt.hasNext()) {
        varIt.next(); // Skip v1
      }
      if (varIt.hasNext()) {
        v2 = varIt.next();
      }

      Stack<Literal*> lits1;
      Stack<Literal*> lits2;

      for (int i = 0; i < (int)cl->length(); ++i) {
        Literal* lit = (*cl)[i];
        DHSet<unsigned> litVars;
        VariableIterator lvit(lit);
        while (lvit.hasNext()) {
          litVars.insert(lvit.next().var());
        }
        if (litVars.contains(v2)) {
          lits2.push(lit);
        } else {
          lits1.push(lit);
        }
      }

      if (!lits1.isEmpty() && !lits2.isEmpty()) {
        static std::map<std::string, unsigned> splitMap;
        std::vector<std::string> litsStr;
        for(unsigned idx=0; idx<lits1.size(); ++idx) {
          litsStr.push_back(lits1[idx]->toString());
        }
        std::sort(litsStr.begin(), litsStr.end());
        std::string canon = "";
        for(const auto& s : litsStr) canon += s + "|";
        
        unsigned pSym;
        if(splitMap.find(canon) != splitMap.end()) {
          pSym = splitMap[canon];
        } else {
          static unsigned splitCounter = 0;
          std::string name = "s2_split_" + std::to_string(++splitCounter);
          pSym = env.signature->addPredicate(name, 0);
          splitMap[canon] = pSym;
        }

        Literal* pPos = Literal::create(pSym, true, {});
        Literal* pNeg = Literal::create(pSym, false, {});

        lits1.push(pPos);
        lits2.push(pNeg);

        Clause* c1 = Clause::fromStack(lits1, cl->inference());
        Clause* c2 = Clause::fromStack(lits2, cl->inference());

        normalizeClauseForS2(c1, outClauses);
        normalizeClauseForS2(c2, outClauses);
        return;
      } else {
        std::cout << "[DEBUG] FAILED TO SPLIT! lits1.size=" << lits1.size() << " lits2.size=" << lits2.size() << " Clause: " << cl->toString() << std::endl;
      }
    }
  }

  // Check 2: Mixed ground and non-ground literals in a clause (Condition 2 of Def 6)
  bool hasGroundLiteral = false;
  bool hasNonGroundLiteral = false;
  Stack<Literal*> groundLits;
  Stack<Literal*> nonGroundLits;

  for (int i = 0; i < (int)cl->length(); ++i) {
    Literal* lit = (*cl)[i];
    if (lit->ground() && lit->arity() > 0) {
      hasGroundLiteral = true;
      groundLits.push(lit);
    } else {
      if (!lit->ground() || lit->arity() > 0) {
        hasNonGroundLiteral = true;
      }
      nonGroundLits.push(lit);
    }
  }

  if (hasGroundLiteral && hasNonGroundLiteral) {
    static std::map<std::string, unsigned> groundMap;
    std::vector<std::string> litsStr;
    for(unsigned idx=0; idx<groundLits.size(); ++idx) {
      litsStr.push_back(groundLits[idx]->toString());
    }
    std::sort(litsStr.begin(), litsStr.end());
    std::string canon = "";
    for(const auto& s : litsStr) canon += s + "|";
    
    unsigned pSym;
    if(groundMap.find(canon) != groundMap.end()) {
      pSym = groundMap[canon];
    } else {
      static unsigned groundCounter = 0;
      std::string name = "s2_ground_" + std::to_string(++groundCounter);
      pSym = env.signature->addPredicate(name, 0);
      groundMap[canon] = pSym;
    }

    Literal* pPos = Literal::create(pSym, true, {});
    Literal* pNeg = Literal::create(pSym, false, {});

    groundLits.push(pPos);
    nonGroundLits.push(pNeg);

    Clause* c1 = Clause::fromStack(groundLits, cl->inference());
    Clause* c2 = Clause::fromStack(nonGroundLits, cl->inference());

    normalizeClauseForS2(c1, outClauses);
    normalizeClauseForS2(c2, outClauses);
    return;
  }

  outClauses.push(cl);
}

bool Preprocessor::containsAllVariables(const DHSet<unsigned> &vars, const DHSet<unsigned> &required)
{
  FO2Logger::logDebug("checking variable coverage: " + std::to_string(vars.size()) + " vs " + std::to_string(required.size()));
  if (vars.size() < required.size()) {
    return false;
  }

  for (DHSet<unsigned>::Iterator it(required); it.hasNext();) {
    if (!vars.contains(it.next())) {
      return false;
    }
  }

  return true;
}

bool Preprocessor::coversAllVariables(const Term *t, const DHSet<unsigned> &clauseVars)
{
  DHSet<unsigned> termVars;
  VariableIterator vit(t);
  while (vit.hasNext()) {
    TermList v = vit.next();
    termVars.insert(v.var());
  }
  return Preprocessor::containsAllVariables(termVars, clauseVars);
}

bool Preprocessor::validateClause(Clause *cl, const char *&errorMessage)
{
  DHSet<unsigned> clauseVars;
  VirtualIterator<unsigned> vit = cl->getVariableIterator();
  while (vit.hasNext()) {
    clauseVars.insert(vit.next());
  }
  if (clauseVars.size() > 2) {
    errorMessage = "FO2Preprocessor S2 Definition 6 (1): clause contains more than 2 variables.";
    std::cout << "[DEBUG] Clause failing Condition 6(1) (>2 vars): " << cl->toString() << std::endl;
    return false;
  }

  bool hasGroundLiteral = false;
  bool hasNonGroundLiteral = false;
  bool hasTwoVarLiteral = false;

  for (int i = 0; i < (int)cl->length(); ++i) {
    Literal *lit = (*cl)[i];
    DHSet<unsigned> litVars;
    VariableIterator lvit(lit);
    while (lvit.hasNext()) {
      litVars.insert(lvit.next().var());
    }

    if (litVars.size() == 0 && lit->arity() > 0) {
      hasGroundLiteral = true;
    } else if (litVars.size() > 0) {
      hasNonGroundLiteral = true;
    }

    if (clauseVars.size() == 2 && litVars.size() == 2) {
      hasTwoVarLiteral = true;
    }

    // Definition 6 (1): c contains no nested function symbols
    unsigned arity = lit->arity();
    for (unsigned a = 0; a < arity; ++a) {
      const TermList *tl = lit->nthArgument(a);
      if (tl->isTerm()) {
        const Term *t = tl->term();
        if (!t->isShallow()) {
          errorMessage = "FO2Preprocessor S2 Definition 6 (1): clause contains nested function symbols.";
          return false;
        }
        // Definition 6 (3): Each functional term in c contains all variables occurring in c
        if (!t->ground() && !Preprocessor::coversAllVariables(t, clauseVars)) {
          errorMessage = "FO2Preprocessor S2 Definition 6 (3): functional term does not contain all variables of c.";
          return false;
        }
      }
    }
  }

  // Definition 6 (2): If c contains ground literals, then it is a ground clause.
  if (hasGroundLiteral && hasNonGroundLiteral) {
    errorMessage = "FO2Preprocessor S2 Definition 6 (2): clause containing ground literal is not entirely ground.";
    return false;
  }

  // Definition 6 (4): There is a literal in c that contains all variables of c
  if (clauseVars.size() == 2 && !hasTwoVarLiteral) {
    errorMessage = "FO2Preprocessor S2 Definition 6 (4): 2-variable clause has no literal containing all variables of c.";
    std::cout << "[DEBUG] Clause failing Condition 6(4): " << cl->toString() << std::endl;
    return false;
  }

  return true;
}

bool Preprocessor::preprocess(Problem &prb)
{
  FO2Logger::logPhase("Starting FO2 Preprocessing");

  // Phase 1 & 2: NNF, Naming, Flattening, Skolemization and Clausification (CNF)
  UnitList::DelIterator it(prb.units());
  Stack<Clause*> clauses;
  Shell::NewCNF newCnf(0);
  Shell::Naming naming(1, false, false);
  Stack<Clause*> allClauses;

  while (it.hasNext()) {
    Unit *u = it.next();

    if (u->isClause()) {
      allClauses.push(static_cast<Clause*>(u));
      it.del();
      continue;
    }

    FormulaUnit *fu = static_cast<FormulaUnit *>(u);
      if (FO2Logger::showsDebug()) {
        FO2Logger::logDebug("processing unit " + fu->toString());
      }
      fu = Shell::Rectify::rectify(fu);
      fu = Shell::NNF::nnf(fu);
      fu = Shell::SimplifyFalseTrue::simplify(fu);

      if (fu->formula()->connective() == FALSE) {
        Clause* emptyCl = Clause::fromStack(Stack<Literal*>(), NonspecificInference1(InferenceRule::INPUT, fu));
        allClauses.push(emptyCl);
        it.del();
        continue;
      }
      if (fu->formula()->connective() == TRUE) {
        it.del();
        continue;
      }

      // Disabling Naming for FO2: Naming extracts subformulas into external definitions. 
      // This interferes with the exact variable scopes in FO2 and causes Skolemization 
      // to generate >2 variables for formulas (like the one in Lemma 1).
      UnitList* defs = nullptr;
      // fu = naming.apply(fu, defs);
      fu = Shell::Flattening::flatten(fu);
      fu = Shell::Skolem::skolemise(fu);

      clauses.reset();
      newCnf.clausify(fu, clauses);
      while (!clauses.isEmpty()) {
        allClauses.push(clauses.pop());
      }

      if (defs) {
        UnitList::Iterator defIt(defs);
        while (defIt.hasNext()) {
          Unit* defU = defIt.next();
          if (!defU->isClause()) {
            FormulaUnit* defFu = static_cast<FormulaUnit*>(defU);
            defFu = Shell::Rectify::rectify(defFu);
            defFu = Shell::NNF::nnf(defFu);
            defFu = Shell::SimplifyFalseTrue::simplify(defFu);

            if (defFu->formula()->connective() == FALSE) {
              Clause* emptyCl = Clause::fromStack(Stack<Literal*>(), NonspecificInference1(InferenceRule::INPUT, defFu));
              allClauses.push(emptyCl);
              continue;
            }
            if (defFu->formula()->connective() == TRUE) {
              continue;
            }

            defFu = Shell::Flattening::flatten(defFu);
            defFu = Shell::Skolem::skolemise(defFu);

            clauses.reset();
            newCnf.clausify(defFu, clauses);
            while (!clauses.isEmpty()) {
              allClauses.push(clauses.pop());
            }
          } else {
            allClauses.push(static_cast<Clause*>(defU));
          }
        }
      }
      it.del();
  }

  Stack<Clause*> normalizedClauses;
  while (!allClauses.isEmpty()) {
    normalizeClauseForS2(allClauses.pop(), normalizedClauses);
  }

  while (!normalizedClauses.isEmpty()) {
    UnitList::push(normalizedClauses.pop(), prb.units());
  }

  // Phase 3: Validation of S2 constraints on all resulting clauses
  bool valid = true;
  UnitList::Iterator uit(prb.units());
  while (uit.hasNext()) {
    Unit *u = uit.next();
    if (u->isClause()) {
      Clause *cl = static_cast<Clause *>(u);
      const char *errorMessage = nullptr;
      FO2Logger::logDebug("entering clause validation");
      if (!validateClause(cl, errorMessage)) {
        FO2Logger::logDebug("FO2Preprocessor Validation Warning: " + std::string(errorMessage));
        valid = false;
      }
    }
  }

  if (!valid) {
    FO2Logger::logDebug("FO2Preprocessor: Some pre-saturation clauses contain components that violate S2 constraints.");
  } else {
    FO2Logger::logLemma("PROBLEM AFTER PREPROCESSING", prb);
    FO2Logger::logPhase("FO2 Preprocessing completed");
  }
  return valid;
}
} // namespace FO2Preprocessor
