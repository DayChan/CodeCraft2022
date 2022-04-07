// #define DEBUG
// 提交时记得注释掉这个宏
#include "ContestIO.h"
#include "Calculate.h"

int main() {
    // 处理输入
    ContestIO io;
    io.handle_contest_input();
    std::vector<double> costs = {1.0};
    // std::vector<double> costs = {175 * io.base_cost, 180 * io.base_cost, 200 * io.base_cost, 225 * io.base_cost};
    // std::vector<double> costs = {0.8 * io.base_cost, 1.0 * io.base_cost, 2.0 * io.base_cost, 3.0 * io.base_cost, 4.0 * io.base_cost};
    // std::vector<double> costs = {1.0};
    int min_score = INT32_MAX;
    std::vector<std::vector<std::vector<std::vector<std::pair<int, std::string>>>>> min_res;
    for(auto cost : costs) {
        ContestCalculate* cal = new ContestCalculate(io);

        // cal->brute_force5();
        cal->brute_force5_with_edge_limit(cost);
        // cal->brute_force3_with_coeffiicient_avg_dist(cost);
        // cal->brute_force3_with_coeffiicient_avg_dist_with_calculate_first_score(cost);
        // cal->brute_force3_with_basecost_dist();
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