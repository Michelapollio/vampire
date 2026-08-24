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
    QUIET = 0, //O: silenzioso, (solo errori critici o output finale)
    PHASES = 1, //1: Avanzamento macro-fasi (Default)
    LEMMATA = 2, //2: Stato del problema prima e dopo ogni Lemma
    DEBUG = 3 //3: Tracciamento dettagliato e interno

};

class FO2Logger {
    public: 
        static void setVerbosity(VerbosityLevel level){
            getVerbosityInternal() = level;
        }

        static VerbosityLevel getVerbosity(){
            return getVerbosityInternal();
        }

        static bool isQuiet(){
            return getVerbosity() == VerbosityLevel::QUIET;
        }

        static bool showsPhases(){
            return static_cast<int>(getVerbosity()) >= static_cast<int>(VerbosityLevel::PHASES);
        }

        static bool showsLemmata(){
            return static_cast<int>(getVerbosity()) >= static_cast<int>(VerbosityLevel::LEMMATA);
        }

        static bool showsDebug(){
            return static_cast<int>(getVerbosity()) >= static_cast<int>(VerbosityLevel::DEBUG);
        }

        // Logs macro-phase messages (level PHASES >= 1)
        static void logPhase(const std::string& msg){
            if(showsPhases()){
                std::cout << "[FO2][PHASE] " << msg << std::endl;
            }
        }

        // Logs complete state of problem units (level LEMMATA >= 2)
        static void logLemma(const std::string& title, Kernel::Problem &prb){
            if (showsLemmata()){
                std::cout << "\n==================================================\n";
                std::cout << "[FO2][LEMMA] " << title << "\n";
                std::cout << "==================================================\n";
            
                Kernel::UnitList::Iterator it(prb.units());
                size_t count = 0;
                while (it.hasNext()) {
                    Kernel::Unit* u = it.next();
                    std::cout << "  [" << ++count << "] " << u->toString() << "\n";
                }
                if (count == 0) {
                    std::cout << "  (No units present in the problem)\n";
                }
                std::cout << "--------------------------------------------------\n\n";
            }
        }

        // Always logs problem units state regardless of verbosity level
        static void logAlways(const std::string& title, Kernel::Problem &prb){
            std::cout << "\n==================================================\n";
            std::cout << "[FO2][FINAL OUTPUT] " << title << "\n";
            std::cout << "==================================================\n";

            Kernel::UnitList::Iterator it(prb.units());
            size_t count = 0;
            while (it.hasNext()) {
                Kernel::Unit* u = it.next();
                std::cout << "  [" << ++count << "] " << u->toString() << "\n";
            }
            if (count == 0) {
                std::cout << "  (No units present in the problem)\n";
            }
            std::cout << "--------------------------------------------------\n\n";
        }

        // Logs detailed debug messages (level DEBUG >= 3)
        static void logDebug(const std::string& msg) {
            if (showsDebug()) {
                std::cout << "[FO2][DEBUG] " << msg << std::endl;
            }
        }
    
    private :
        static VerbosityLevel& getVerbosityInternal() {
            static VerbosityLevel currentLevel = VerbosityLevel::PHASES; 
            return currentLevel;   
        }
    };

}// namespace FO2Fragment

#endif // FO2LOGGER_HPP