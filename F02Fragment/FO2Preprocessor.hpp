
#include "Kernel/Problem.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Shell/NNF.hpp"
#include "Shell/Flattening.hpp"
#include "Shell/Skolem.hpp"

using namespace std;
using namespace Shell;

class Preprocessor {
    public:
    static void preprocess(Problem& prb);
    static bool containsAllVariables(const DHSet<unsigned>& vars, const DHSet<unsigned>& required);
    static bool coversAllVariables(const Term* t, const DHSet<unsigned>& clauseVars);
    static bool hasMaximalLiteral(Clause* cl, const DHSet<unsigned>& clauseVars);
    static bool validateClause(Clause* cl, const char*& errorMessage);

};
