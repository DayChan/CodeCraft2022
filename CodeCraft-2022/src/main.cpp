// #define DEBUG
// 提交时记得注释掉这个宏
#include "ContestIO.h"
#include "Calculate.h"

int main() {
    // 处理输入
    ContestIO io;
    io.handle_contest_input();
    
    std::vector<int> costs = {int(2.3 * io.base_cost), int(2.4 * io.base_cost), int(2.5 * io.base_cost), int(2.6 * io.base_cost)};
    int min_score = INT32_MAX;
    std::vector<std::vector<std::vector<std::vector<std::pair<int, std::string>>>>> min_res;
    for(auto cost : costs) {
        ContestCalculate* cal = new ContestCalculate(io);
        cal->brute_force3_with_more_basecost_dist(cost);
        int score = cal->calculate_94_score();
        #ifdef DEBUG
        std::cout << "COST:" << cost << " SCORE: " << score << std::endl;
        #endif
        if(score < min_score) {
            min_score = score;
            min_res = cal->res;
        }
        delete cal;
    }
    
    io.handle_output(min_res);
    // cal.average_distribute();
    // cal.res_redist();
    // cal.handle_output();
    
    
}