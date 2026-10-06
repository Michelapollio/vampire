// Standalone check of FO2Inferences::subsumes (consistent substitution + backtracking).
// Release-safe: does not rely on ASS.
#include <cstdlib>
#include <iostream>

#include "Kernel/Clause.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Signature.hpp"
#include "Lib/Environment.hpp"
#include "F02Fragment/Resolution/FO2Resolution.hpp"
#include "F02Fragment/Resolution/FO2Inferences.hpp"

using namespace Kernel;
using namespace FO2Fragment;

static Clause* mk(std::initializer_list<Literal*> lits)
{
  return Clause::fromLiterals(lits, Inference(FromInput(UnitInputType::AXIOM)));
}

static int failures = 0;
#define EXPECT(cond, expected)                                                       \
  do {                                                                               \
    bool got = (cond);                                                               \
    std::cout << (got == (expected) ? "[ok]   " : "[FAIL] ") << #cond << " -> "     \
              << got << " (expected " << (expected) << ")" << std::endl;             \
    if (got != (expected)) failures++;                                               \
  } while (0)

int main()
{
  TermList x = TermList::var(0);
  unsigned P = env.signature->addFreshPredicate(1, "P");
  unsigned Q = env.signature->addFreshPredicate(1, "Q");
  TermList a(Term::createConstant(env.signature->addFreshFunction(0, "a")));
  TermList b(Term::createConstant(env.signature->addFreshFunction(0, "b")));

  auto L = [](unsigned f, TermList t) { return Literal::create1(f, true, t); };

  IndexedClause sub = IndexedClause::fromClause(mk({L(P, x), L(Q, x)}));
  IndexedClause diff = IndexedClause::fromClause(mk({L(P, a), L(Q, b)}));
  IndexedClause same = IndexedClause::fromClause(mk({L(P, a), L(Q, a)}));
  IndexedClause back = IndexedClause::fromClause(mk({L(P, a), L(P, b), L(Q, b)}));

  EXPECT(FO2Inferences::subsumes(sub, diff), false);  // P(x)|Q(x) vs P(a)|Q(b)
  EXPECT(FO2Inferences::subsumes(sub, same), true);   // P(x)|Q(x) vs P(a)|Q(a)
  EXPECT(FO2Inferences::subsumes(sub, back), true);   // needs backtracking x->a fails, x->b ok

  // ---- Factoring: same polarity AND same index required ----
  TermList y = TermList::var(1);
  unsigned R = env.signature->addFreshPredicate(2, "R");
  TermList xy[2] = {x, y}, yx[2] = {y, x}, xx[2] = {x, x};
  IndexedClause out;

  // R(x,y):1 | R(y,x):1 -> same index (1), same polarity, unifiable => factor
  IndexedClause f1 = IndexedClause::fromClause(
      mk({Literal::create(R, 2, true, xy), Literal::create(R, 2, true, yx)}));
  EXPECT(FO2Inferences::factor(f1, 0, 1, out), true);

  // R(x,y):1 | R(x,x):0 -> unifiable and same polarity but DIFFERENT index => no factor
  IndexedClause f2 = IndexedClause::fromClause(
      mk({Literal::create(R, 2, true, xy), Literal::create(R, 2, true, xx)}));
  EXPECT(f2[0].index != f2[1].index, true);
  EXPECT(FO2Inferences::factor(f2, 0, 1, out), false);

  // R(x,y):1 | ~R(y,x):1 -> same index but DIFFERENT polarity => no factor
  IndexedClause f3 = IndexedClause::fromClause(
      mk({Literal::create(R, 2, true, xy), Literal::create(R, 2, false, yx)}));
  EXPECT(FO2Inferences::factor(f3, 0, 1, out), false);

  std::cout << (failures ? "FAILED" : "ALL PASSED") << std::endl;
  return failures ? 1 : 0;
}
