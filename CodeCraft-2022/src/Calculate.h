#pragma once
#include "ContestIO.h"

class ContestCalculate {
    public:
        ContestCalculate() {
            // 处理输入
            io.handle_contest_input();
            // 对于所有时间点，复制一份边缘节点带宽，下面作为剩余带宽的记录
            sb_map_alltime.resize(io.data_dm_rowstore.size(), io.sb_map);
        }

        void brute_force() {
            res.resize(
                io.data_dm_rowstore.size(),
                std::vector<std::vector<std::vector<std::string>>>(
                    io.client_names.size(),
                    std::vector<std::vector<std::string>>(io.edges_names.size())));  

            // 边缘节点分配的时间
            std::vector<int> edge_times(io.edges_names.size(), 0);
            
            // 百分之五的时间点个数
            int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

            // 已经超分配到边
            std::vector<bool> edge_exceed(io.edges_names.size(), false);
            
            // edge节点超分配的最大值
            std::vector<int> edge_max_record(io.edges_names.size(), 0);

            // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
            // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
            std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
                io.edges_names.size(),
                std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(),
                                                {0, 0}));

            // 开始第一轮分配，第一轮分配是面向边缘节点分配
            // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
            std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
            for (size_t i = 0; i < io.edges_names.size(); i++) {
                sort_edge_avg_dist_highest_value.push_back(
                    {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
            }
            // 从高到低排
            std::sort(sort_edge_avg_dist_highest_value.begin(), sort_edge_avg_dist_highest_value.end(),
                    std::greater<std::pair<double, int>>());

            for (int i = 0; i < io.edges_names.size(); i++) {
                int edge_idx = sort_edge_avg_dist_highest_value[i].second;
                // 选取排序存储slot
                std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
                // 边缘节点能被分配到的客户端节点
                auto& clis = io.edge_dist_clients[edge_idx];
                // 排序每个边缘节点能够分配到的客户端的需求总值
                for (int j = 0; j < io.data_dm_rowstore.size(); j++) {
                    std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[j];
                    int sumrow = 0;
                    for (auto cli : clis) {
                        auto& cli_streams = row[cli];
                        for(auto& cli_stream : cli_streams) {
                            sumrow += cli_stream.first;
                        }
                    }
                    sort_require[j] = {sumrow, j};
                }
                // 也是从高到低排
                std::sort(sort_require.begin(), sort_require.end(), std::greater<std::pair<int, int>>());

                int five_count = 0; // 边缘节点分配次数的计数
                int require_idx = 0; // 需求排序后遍历的idx

                while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                    int time = sort_require[require_idx].second;
                    // 获取当前时间点边缘节点剩余的带宽
                    auto& sb_map_ref = sb_map_alltime[time];
                    std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[time];
                    // 计算当前edge节点在当前时间点分配了多少
                    int dist_v_acc = 0;

                    // 根据客户端的连通性排序当前节点可分配的客户端
                    std::vector<std::pair<int, int>> sort_cli_edge_nums(clis.size());
                    for (int j = 0; j < clis.size(); j++) {
                        int cli_edge_nums = io.qos_map[clis[j]].size();
                        sort_cli_edge_nums[j] = {cli_edge_nums, clis[j]};
                    }
                    // 从低到高排序
                    std::sort(sort_cli_edge_nums.begin(), sort_cli_edge_nums.end());

                    for (auto& cli : sort_cli_edge_nums) {
                        int cli_idx = cli.second;
                        for(auto& cli_stream_dm : row[cli_idx]) {
                            int& cli_dm = cli_stream_dm.first;
                            if(cli_dm == 0) continue; // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                            // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                            // TODO:对节点的流需求进行排序
                            int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                            int dist_v = 0;
                            if(edge_rest_sb >= cli_dm) {
                                dist_v = cli_dm;
                            } else {
                                dist_v = 0;
                            }
                            if(dist_v) {
                                edge_rest_sb -= dist_v;
                                cli_dm -= dist_v;
                                dist_v_acc += dist_v;
                                res[time][cli_idx][edge_idx].push_back(cli_stream_dm.second);
                            }
                        }
                    }
                    if (dist_v_acc != 0) five_count++;
                    require_idx++;
                }
            }
            // 以上5%分配完 

            // 开始第二轮分配，第二轮分配是面向客户节点分配
            // 对所有客户节点总需求量排序，以此选择时间点
            std::vector<std::pair<int, int>> sort_time(io.data_dm_rowstore.size(), {0, 0});  

            for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
                int sumrow = 0;
                for (auto cli_streams : row) {
                    for(auto cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_time[i] = {sumrow, i};
            }
            // 从高到低排
            std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

            for (size_t st = 0; st < sort_time.size(); st++) {
                int time = sort_time[st].second;

                // 根据时间点选客户端需求行
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

                // 获取edge剩余带宽
                auto& sb_map_ref = sb_map_alltime[time];
                // 记录edge在本轮是否被分配
                std::vector<int> edge_used(io.edges_names.size(), 0);
                // edge当前轮次分配额的累计
                std::vector<int> edge_record_acm(io.edges_names.size(), 0);  
                
                // 根据客户端节点的连通性排序
                std::vector<std::pair<int, int>> sort_cli_dm_in(io.client_names.size());
                for (int j = 0; j < io.client_names.size(); j++) {
                    int cli_edge_nums = io.qos_map[j].size();
                    sort_cli_dm_in[j] = {cli_edge_nums, j};
                }
                // 从低到高排
                std::sort(sort_cli_dm_in.begin(), sort_cli_dm_in.end());

                for (auto& cli : sort_cli_dm_in) {
                    int cli_idx = cli.second;
                    std::vector<std::pair<int, std::string>> cli_streams = row[cli_idx];

                    // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                    std::vector<std::tuple<int, int, int>> sort_array;
                    for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                        sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                            io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                            io.qos_map[cli_idx][i]});
                    }
                    // 注意这里是从高到低排，跟之前不一样
                    std::sort(sort_array.begin(), sort_array.end(),
                            [](const std::tuple<int, int, int>& i, const std::tuple<int, int, int>& j) {
                                return i > j;
                            });

                    // 使用已经超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge从未被超分配过，直接跳过
                        if (!edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for(auto& cli_stream : cli_streams) {
                            // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if(cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if(edge_rest_sb >= cli_stream.first) {
                                // 分配值为客户端需求
                                dist_v = cli_stream.first;
                                // 减去边缘节点剩余带宽
                                edge_rest_sb -= dist_v;
                                // 客户节点当前流的需求置0
                                cli_stream.first = cli_stream.first - dist_v;
                                // 边缘节点当前时间点已分配的值
                                edge_record_acm[edge_idx] += dist_v;
                                // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                                edge_max_record[edge_idx] =
                                    std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                                res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                    }

                    // 使用未超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                        if (edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for(auto& cli_stream : cli_streams) {
                             // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if(cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if(edge_rest_sb >= cli_stream.first) {
                                // 分配值为客户端需求
                                dist_v = cli_stream.first;
                                // 减去边缘节点剩余带宽
                                edge_rest_sb -= dist_v;
                                // 客户节点当前流的需求置0
                                cli_stream.first -= dist_v;
                                // 边缘节点当前时间点已分配的值
                                edge_record_acm[edge_idx] += dist_v;
                                // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                                edge_max_record[edge_idx] =
                                    std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                                res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                    }
                }

                for (size_t i = 0; i < edge_times.size(); i++) {
                    // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                    edge_times[i] += edge_used[i];
                }
            }
            return;
        }

        /* 计分函数 */
        int calculate_94_score() {
            int pos_94 = 0.05 * res.size();
            std::vector<int> edge_score_94(io.edges_names.size());
            std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
                io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

            for (int time = 0; time < res.size(); time++) {
                for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                    int edge_dist = io.sb_map[io.edges_names[edge_idx]] - sb_map_alltime[time][io.edges_names[edge_idx]];
                    edge_sort_score[edge_idx][time] = {edge_dist, time};
                }
            }
            double sum = 0;
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                auto edge_score = edge_sort_score[edge_idx];
                std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
                edge_score_94[edge_idx] = edge_score[pos_94].first;
                if(edge_score_94[edge_idx] == 0) {
                    sum += 0;
                } 
                else if(edge_score_94[edge_idx] < io.base_cost) {
                    sum += io.base_cost;
                } else {
                    int over = edge_score_94[edge_idx] - io.base_cost;
                    sum += io.base_cost;
                    sum += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
                }
                sum += 0.5;
                // sum += edge_score_94[edge_idx];
                // std::cout << "edge_idx: " << edge_idx << " score: " << edge_score[pos_94].first << "
                // time: " << edge_score[pos_94].second << std::endl;
            }

            return sum;
        }

        /* 处理输出 */
        void handle_output() {
            io.handle_output(res);
        }
    public:
        ContestIO io;
        std::vector<std::unordered_map<std::string, int>> sb_map_alltime;
        // 时间 / 客户端 / 边缘节点 / 对当前客户端那些流进行了分配
        std::vector<std::vector<std::vector<std::vector<std::string>>>> res; 

};
