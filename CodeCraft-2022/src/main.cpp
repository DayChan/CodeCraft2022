// #define DEBUG
// 提交时记得注释掉这个宏
#include "ContestIO.h"
#include "Calculate.h"
int main() {
    ContestCalculate cal;
    cal.brute_force4();
    cal.handle_output();
    #ifdef DEBUG
    std::cout << "SCORE: " << cal.calculate_94_score() << std::endl;
    #endif
}