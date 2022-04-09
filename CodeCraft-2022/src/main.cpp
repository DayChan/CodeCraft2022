#define DEBUG
// 提交时记得注释掉这个宏
#include "ContestIO.h"
#include "Calculate.h"
#include "Timer.h"
int main() {
    // 处理输入
    // Timer timer;
    // timer.set_timeout(290000);

    ContestIO io;
    io.handle_contest_input();
    // std::vector<double> costs = {0.8, 1.0, 1.2};
    // std::vector<double> costs = {175 * io.base_cost, 180 * io.base_cost, 200 * io.base_cost, 225 * io.base_cost};
    // std::vector<double> costs = {0.8 * io.base_cost, 1.0 * io.base_cost, 2.0 * io.base_cost, 3.0 * io.base_cost, 4.0 * io.base_cost};
    // std::vector<double> costs = {0.81, 0.82, 0.83, 0.84, 0.85, 0.86, 0.87, 0.88, 0.89};
    std::vector<double> costs = {1.0};
    // std::vector<double> costs = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 1.0};
    // std::vector<double> costs = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 1.0};
    // std::vector<double> costs = {0.92};
    int min_score = INT32_MAX;
    std::vector<std::vector<std::vector<std::vector<std::pair<int, std::string>>>>> min_res;
    for(auto cost : costs) {
        ContestCalculate* cal = new ContestCalculate(io);

        // cal->brute_force6();
        // cal->brute_force5_with_edge_limit(cost);
        // cal->brute_force3_with_coeffiicient_avg_dist(cost);
        // cal->brute_force3_with_coeffiicient_avg_dist_with_calculate_first_score(cost);
        // cal->brute_force3_with_basecost_dist();
        // cal->brute_force5();
        // int status = cal->brute_force10(cost, 0.5);
        int status = cal->brute_force11(cost, 0.5, 0.3, 0.00, 2);
        int score = 0;
        if(status == -1) {
            score = -1;
            std::cout << "ERROR COST:" << cost << " SCORE: " << score << std::endl;
        }
        else {
            #ifdef DEBUG
            if(status == 0)
            score = cal->calculate_94_score();
            std::cout << "BEFORE COST:" << cost << " SCORE: " << score << std::endl;
            #endif
            for(int i=0; i<5; i++) {
                cal->res_redist();
                cal->res_redist3();
                cal->res_redist4();
                cal->res_redist3();
            }
            
            // cal->output_edge_dist("./output/edge_dist1.txt");
            // cal->brute_force7_with_edge_limit(0.9, cost);
            score = cal->calculate_94_score();
            #ifdef DEBUG
            std::cout << "AFTER REDIST COST:" << cost << " SCORE: " << score << std::endl;
            #endif
        }
        
        if(score >= 0 && score < min_score) {
            min_score = score;
            min_res = cal->res;
            io.handle_output(min_res);
        }
        delete cal;
    }
    
    // cal.average_distribute();
    // cal.res_redist();
    // cal.handle_output();
    
}