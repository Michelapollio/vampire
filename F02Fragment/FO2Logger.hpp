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

        //stampa per livello PHASES (>= 1)
        static void logPhase(const std::string& msg){
            if(showsPhases()){
                std::cout << "[FO2][PHASE] " << msg << std::endl;
            }
        }

        //stampa per lo stato completo delle unità del problema per livello LEMMATA
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
                    std::cout << "  (Nessuna unita' presente nel problema)\n";
                }
                std::cout << "--------------------------------------------------\n\n";
            }
        }

        // stampa sempre lo stato delle unita' del problema a prescindere dal livello di verbosita'
        static void logAlways(const std::string& title, Kernel::Problem &prb){
            std::cout << "\n==================================================\n";
            std::cout << "[FO2][OUTPUT FINALE] " << title << "\n";
            std::cout << "==================================================\n";

            Kernel::UnitList::Iterator it(prb.units());
            size_t count = 0;
            while (it.hasNext()) {
                Kernel::Unit* u = it.next();
                std::cout << "  [" << ++count << "] " << u->toString() << "\n";
            }
            if (count == 0) {
                std::cout << "  (Nessuna unita' presente nel problema)\n";
            }
            std::cout << "--------------------------------------------------\n\n";
        }

        //stampa completa -Debug 
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