#ifndef FO2LOGGER_HPP
#define FO2LOGGER_HPP

#include <iostream>
#include <string>

#include "Kernel/Problem.hpp"
#include "Kernel/Unit.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/FormulaUnit.hpp"

namespace FO2Fragment {

enum class VerbosityLevel {
    QUIET = 0,
    PHASES = 1,
    LEMMATA = 2,
    DEBUG = 3
};

class FO2Logger {
public:
    static void setVerbosity(VerbosityLevel level) {
        getVerbosityInternal() = level;
    }

    static VerbosityLevel getVerbosity() {
        return getVerbosityInternal();
    }

    static bool isQuiet() {
        return getVerbosity() == VerbosityLevel::QUIET;
    }

    static bool showsPhases() {
        return static_cast<int>(getVerbosity()) >= static_cast<int>(VerbosityLevel::PHASES);
    }

    static bool showsLemmata() {
        return static_cast<int>(getVerbosity()) >= static_cast<int>(VerbosityLevel::LEMMATA);
    }

    static bool showsDebug() {
        return static_cast<int>(getVerbosity()) >= static_cast<int>(VerbosityLevel::DEBUG);
    }

    static void logPhase(const std::string& msg) {
        if (showsPhases()) {
            std::cout << "[FO2][PHASE] " << msg << std::endl;
        }
    }

    static void logLemma(const std::string& title, Kernel::Problem& problem) {
        if (!showsLemmata()) {
            return;
        }

        std::cout << "\n==================================================\n";
        std::cout << "[FO2][LEMMA] " << title << "\n";
        std::cout << "==================================================\n";
        writeProblemUnits(problem);
        std::cout << "--------------------------------------------------\n\n";
    }

    static void logAlways(const std::string& title, Kernel::Problem& problem) {
        if (isQuiet()) {
            return;
        }

        std::cout << "\n==================================================\n";
        std::cout << "[FO2][FINAL OUTPUT] " << title << "\n";
        std::cout << "==================================================\n";
        writeProblemUnits(problem);
        std::cout << "--------------------------------------------------\n\n";
    }

    static void logDebug(const std::string& msg) {
        if (showsDebug()) {
            std::cout << "[FO2][DEBUG] " << msg << std::endl;
        }
    }

private:
    static void writeProblemUnits(Kernel::Problem& problem) {
        Kernel::UnitList::Iterator it(problem.units());
        size_t count = 0;
        while (it.hasNext()) {
            Kernel::Unit* unit = it.next();
            if (unit->isClause()) {
                std::cout << "  [" << ++count << "] "
                          << static_cast<Kernel::Clause*>(unit)->toNiceString() << "\n";
            } else {
                std::cout << "  [" << ++count << "] " << unit->toString() << "\n";
            }
        }
        if (count == 0) {
            std::cout << "  (No units present in the problem)\n";
        }
    }

    static VerbosityLevel& getVerbosityInternal() {
        static VerbosityLevel currentLevel = VerbosityLevel::PHASES;
        return currentLevel;
    }
};

} // namespace FO2Fragment

#endif // FO2LOGGER_HPP