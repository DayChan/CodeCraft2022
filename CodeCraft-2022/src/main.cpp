// #define DEBUG
// 提交时记得注释掉这个宏
#include "ContestIO.h"
#include "Calculate.h"
int main() {
    ContestCalculate cal;
    cal.brute_force3_with_basecost_dist();
    // cal.average_distribute();
    // cal.res_redist();
    cal.handle_output();
    #ifdef DEBUG
    std::cout << "SCORE: " << cal.calculate_94_score() << std::endl;
    #endif
}