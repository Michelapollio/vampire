
#include "Kernel/Problem.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Shell/NNF.hpp"
#include "Shell/Flattening.hpp"
#include "Shell/Skolem.hpp"
#include "Shell/NewCNF.hpp"
#include "Shell/CNF.hpp"

namespace FO2Preprocessor {

  using namespace Shell;
  using namespace Kernel;

  /**
   * @brief Preprocessor for the FO2 fragment.
   * Performs NNF transformation, formula flattening, Skolemization,
   * CNF clausification, and validation of S2 constraints.
   */
  class Preprocessor {
  public:
    /**
     * @brief Preprocesses an FO2 problem into CNF form and validates S2 constraints.
     * @param prb The problem instance to preprocess.
     */
    static void preprocess(Problem &prb);

  private:
    /**
     * @brief Checks if a variable set contains all required variables.
     * @param vars Set of available variables.
     * @param required Set of required variables.
     * @return true if vars covers all variables in required, false otherwise.
     */
    static bool containsAllVariables(const DHSet<unsigned> &vars, const DHSet<unsigned> &required);

    /**
     * @brief Checks if a term covers all variables in a clause.
     * @param t Pointer to the term.
     * @param clauseVars Set of variables present in the clause.
     * @return true if the term covers all clause variables, false otherwise.
     */
    static bool coversAllVariables(const Term *t, const DHSet<unsigned> &clauseVars);

    /**
     * @brief Validates S2 variable and functional term constraints on a clause.
     * @param cl Pointer to the clause to validate.
     * @param errorMessage Output error message if validation fails.
     * @return true if the clause satisfies S2 constraints, false otherwise.
     */
    static bool validateClause(Clause *cl, const char *&errorMessage);
  };
} // namespace FO2Preprocessor