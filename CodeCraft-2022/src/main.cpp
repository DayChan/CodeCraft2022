#define DEBUG
#include "ContestIO.h"
#include "Calculate.h"
int main() {
    ContestCalculate cal;
    cal.brute_force();
    cal.handle_output();
    std::cout << "SCORE: " << cal.calculate_94_score() << std::endl;
}