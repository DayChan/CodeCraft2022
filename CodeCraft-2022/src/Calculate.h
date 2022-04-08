#pragma once
#include <algorithm>
#include <cmath>

#include "ContestIO.h"

class ContestCalculate {
   public:
    ContestCalculate(ContestIO io_input) {
        this->io = io_input;

        // 对于所有时间点，复制一份边缘节点带宽，下面作为剩余带宽的记录
        sb_map_alltime.resize(io.data_dm_rowstore.size(), io.sb_map);
    }

    // brute_force训练赛分数为1011997
    void brute_force() {
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));

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
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
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
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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

    // brute_force2训练赛分数668546
    // 当前第二轮分配使用暴力直接分而非优先分配已经分过的节点，那种方法在训练赛成绩为671912，可以取消注释重现
    void brute_force2() {
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));

        // 进行两次分配，节点在第一次的分配中被分配过
        std::vector<bool> edge_alloc_in_first(io.edges_names.size());
        {
            auto sb_map_alltime_in = this->sb_map_alltime;
            auto data_dm_rowstore = io.data_dm_rowstore;
            // 边缘节点分配的时间
            std::vector<int> edge_times(io.edges_names.size(), 0);

            // 百分之五的时间点个数
            int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;

            // 已经超分配到边
            std::vector<bool> edge_exceed(io.edges_names.size(), false);

            // edge节点超分配的最大值
            std::vector<int> edge_max_record(io.edges_names.size(), 0);

            // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
            // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
            std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
                io.edges_names.size(),
                std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));

            // 开始第一轮分配，第一轮分配是面向边缘节点分配
            // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
            std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
            for (size_t i = 0; i < io.edges_names.size(); i++) {
                sort_edge_avg_dist_highest_value.push_back(
                    {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
            }
            // 从高到低排
            std::sort(sort_edge_avg_dist_highest_value.begin(),
                      sort_edge_avg_dist_highest_value.end(),
                      std::greater<std::pair<double, int>>());

            for (int i = 0; i < io.edges_names.size(); i++) {
                int edge_idx = sort_edge_avg_dist_highest_value[i].second;
                // 选取排序存储slot
                std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
                // 边缘节点能被分配到的客户端节点
                auto& clis = io.edge_dist_clients[edge_idx];
                // 排序每个边缘节点能够分配到的客户端的需求总值
                for (int j = 0; j < data_dm_rowstore.size(); j++) {
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[j];
                    int sumrow = 0;
                    for (auto cli : clis) {
                        auto& cli_streams = row[cli];
                        for (auto& cli_stream : cli_streams) {
                            sumrow += cli_stream.first;
                        }
                    }
                    sort_require[j] = {sumrow, j};
                }
                // 也是从高到低排
                std::sort(sort_require.begin(), sort_require.end(),
                          std::greater<std::pair<int, int>>());

                int five_count = 0;   // 边缘节点分配次数的计数
                int require_idx = 0;  // 需求排序后遍历的idx

                while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                    int time = sort_require[require_idx].second;
                    // 获取当前时间点边缘节点剩余的带宽
                    auto& sb_map_ref = sb_map_alltime_in[time];
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[time];
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
                        std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                                  std::greater<std::pair<int, std::string>>());
                        for (auto& cli_stream_dm : row[cli_idx]) {
                            int& cli_dm = cli_stream_dm.first;
                            if (cli_dm == 0)
                                continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                            // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                            // TODO:对节点的流需求进行排序

                            int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                            int dist_v = 0;
                            if (edge_rest_sb >= cli_dm) {
                                dist_v = cli_dm;
                            } else {
                                dist_v = 0;
                            }
                            if (dist_v) {
                                edge_rest_sb -= dist_v;
                                cli_dm -= dist_v;
                                dist_v_acc += dist_v;
                                // res[time][cli_idx][edge_idx].push_back(cli_stream_dm.second);
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
            std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

            for (int i = 0; i < data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = data_dm_rowstore[i];
                int sumrow = 0;
                for (auto cli_streams : row) {
                    for (auto cli_stream : cli_streams) {
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
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

                // 获取edge剩余带宽
                auto& sb_map_ref = sb_map_alltime_in[time];
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
                    std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                    std::sort(cli_streams.begin(), cli_streams.end(),
                              std::greater<std::pair<int, std::string>>());
                    // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                    std::vector<std::tuple<int, int, int>> sort_array;
                    for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                        sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                              io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                              io.qos_map[cli_idx][i]});
                    }
                    // 注意这里是从高到低排，跟之前不一样
                    std::sort(sort_array.begin(), sort_array.end(),
                              [](const std::tuple<int, int, int>& i,
                                 const std::tuple<int, int, int>& j) { return i > j; });

                    // 使用已经超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge从未被超分配过，直接跳过
                        // if (!edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for (auto& cli_stream : cli_streams) {
                            // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if (cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if (edge_rest_sb >= cli_stream.first) {
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
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                        if (edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    }

                    // // 使用未超分配过的edge进行分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                    //     if (edge_exceed[edge_idx]) continue;
                    //     // 获取当前节点剩余带宽
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     for (auto& cli_stream : cli_streams) {
                    //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = 0;
                    //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                    //         if (edge_rest_sb >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first -= dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             edge_record_acm[edge_idx] += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    //     if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    // }
                }

                for (size_t i = 0; i < edge_times.size(); i++) {
                    // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                    edge_times[i] += edge_used[i];
                }
            }
            edge_alloc_in_first = edge_exceed;
        }

        // !开始正式分配
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
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
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
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                // // 使用未超分配过的edge进行分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                //     if (edge_exceed[edge_idx]) continue;

                //     // 获取当前节点剩余带宽
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     for (auto& cli_stream : cli_streams) {
                //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = 0;
                //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                //         if (edge_rest_sb >= cli_stream.first) {
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first -= dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             edge_record_acm[edge_idx] += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //             res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                //         }
                //     }
                //     // if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                // }
            }

            for (size_t i = 0; i < edge_times.size(); i++) {
                // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                edge_times[i] += edge_used[i];
            }
        }
        return;
    }

    // brute_force3训练赛分数662626
    // brute_force3的思想是，超分配的节点取少数出来2%次数分配满
    void brute_force3() {
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));

        // 进行两次分配，节点在第一次的分配中被分配过
        std::vector<bool> edge_alloc_in_first(io.edges_names.size());
        {
            auto sb_map_alltime_in = this->sb_map_alltime;
            auto data_dm_rowstore = io.data_dm_rowstore;
            // 边缘节点分配的时间
            std::vector<int> edge_times(io.edges_names.size(), 0);

            // 百分之五的时间点个数
            int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;

            // 已经超分配到边
            std::vector<bool> edge_exceed(io.edges_names.size(), false);

            // edge节点超分配的最大值
            std::vector<int> edge_max_record(io.edges_names.size(), 0);

            // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
            // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
            std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
                io.edges_names.size(),
                std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));

            // 开始第一轮分配，第一轮分配是面向边缘节点分配
            // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
            std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
            for (size_t i = 0; i < io.edges_names.size(); i++) {
                sort_edge_avg_dist_highest_value.push_back(
                    {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
            }
            // 从高到低排
            std::sort(sort_edge_avg_dist_highest_value.begin(),
                      sort_edge_avg_dist_highest_value.end(),
                      std::greater<std::pair<double, int>>());

            for (int i = 0; i < io.edges_names.size(); i++) {
                int edge_idx = sort_edge_avg_dist_highest_value[i].second;
                // 选取排序存储slot
                std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
                // 边缘节点能被分配到的客户端节点
                auto& clis = io.edge_dist_clients[edge_idx];
                // 排序每个边缘节点能够分配到的客户端的需求总值
                for (int j = 0; j < data_dm_rowstore.size(); j++) {
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[j];
                    int sumrow = 0;
                    for (auto cli : clis) {
                        auto& cli_streams = row[cli];
                        for (auto& cli_stream : cli_streams) {
                            sumrow += cli_stream.first;
                        }
                    }
                    sort_require[j] = {sumrow, j};
                }
                // 也是从高到低排
                std::sort(sort_require.begin(), sort_require.end(),
                          std::greater<std::pair<int, int>>());

                int five_count = 0;   // 边缘节点分配次数的计数
                int require_idx = 0;  // 需求排序后遍历的idx

                while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                    int time = sort_require[require_idx].second;
                    // 获取当前时间点边缘节点剩余的带宽
                    auto& sb_map_ref = sb_map_alltime_in[time];
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[time];
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
                        std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                                  std::greater<std::pair<int, std::string>>());
                        for (auto& cli_stream_dm : row[cli_idx]) {
                            int& cli_dm = cli_stream_dm.first;
                            if (cli_dm == 0)
                                continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                            // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                            // TODO:对节点的流需求进行排序

                            int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                            int dist_v = 0;
                            if (edge_rest_sb >= cli_dm) {
                                dist_v = cli_dm;
                            } else {
                                dist_v = 0;
                            }
                            if (dist_v) {
                                edge_rest_sb -= dist_v;
                                cli_dm -= dist_v;
                                dist_v_acc += dist_v;
                                // res[time][cli_idx][edge_idx].push_back(cli_stream_dm.second);
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
            std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

            for (int i = 0; i < data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = data_dm_rowstore[i];
                int sumrow = 0;
                for (auto cli_streams : row) {
                    for (auto cli_stream : cli_streams) {
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
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

                // 获取edge剩余带宽
                auto& sb_map_ref = sb_map_alltime_in[time];
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
                    std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                    std::sort(cli_streams.begin(), cli_streams.end(),
                              std::greater<std::pair<int, std::string>>());
                    // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                    std::vector<std::tuple<int, int, int>> sort_array;
                    for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                        sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                              io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                              io.qos_map[cli_idx][i]});
                    }
                    // 注意这里是从高到低排，跟之前不一样
                    std::sort(sort_array.begin(), sort_array.end(),
                              [](const std::tuple<int, int, int>& i,
                                 const std::tuple<int, int, int>& j) { return i > j; });

                    // 使用已经超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge从未被超分配过，直接跳过
                        // if (!edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for (auto& cli_stream : cli_streams) {
                            // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if (cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if (edge_rest_sb >= cli_stream.first) {
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
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                        if (edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    }

                    // // 使用未超分配过的edge进行分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                    //     if (edge_exceed[edge_idx]) continue;
                    //     // 获取当前节点剩余带宽
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     for (auto& cli_stream : cli_streams) {
                    //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = 0;
                    //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                    //         if (edge_rest_sb >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first -= dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             edge_record_acm[edge_idx] += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    //     if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    // }
                }

                for (size_t i = 0; i < edge_times.size(); i++) {
                    // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                    edge_times[i] += edge_used[i];
                }
            }
            edge_alloc_in_first = edge_exceed;
        }

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
                        }
                    }
                }
                if (dist_v_acc != 0) five_count++;
                require_idx++;
            }
        }

        for (int i = 0; i < io.edges_names.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (!edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= second_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
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
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                // // 使用未超分配过的edge进行分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                //     if (edge_exceed[edge_idx]) continue;

                //     // 获取当前节点剩余带宽
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     for (auto& cli_stream : cli_streams) {
                //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = 0;
                //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                //         if (edge_rest_sb >= cli_stream.first) {
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first -= dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             edge_record_acm[edge_idx] += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //             res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                //         }
                //     }
                //     // if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                // }
            }

            for (size_t i = 0; i < edge_times.size(); i++) {
                // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                edge_times[i] += edge_used[i];
            }
        }
        return;
    }

    // brute_force4训练赛分数803869
    // brute_force4与brute_force3不同的是，对节点的排序是用连通性排序
    // brute_force3 与 brute_force4应该结对测试
    void brute_force4() {
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));

        // 进行两次分配，节点在第一次的分配中被分配过
        std::vector<bool> edge_alloc_in_first(io.edges_names.size());
        {
            auto sb_map_alltime_in = this->sb_map_alltime;
            auto data_dm_rowstore = io.data_dm_rowstore;
            // 边缘节点分配的时间
            std::vector<int> edge_times(io.edges_names.size(), 0);

            // 百分之五的时间点个数
            int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;

            // 已经超分配到边
            std::vector<bool> edge_exceed(io.edges_names.size(), false);

            // edge节点超分配的最大值
            std::vector<int> edge_max_record(io.edges_names.size(), 0);

            // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
            // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
            std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
                io.edges_names.size(),
                std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));

            // 开始第一轮分配，第一轮分配是面向边缘节点分配
            // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
            std::vector<std::pair<int, int>> sort_edge_avg_dist_highest_value;
            for (size_t i = 0; i < io.edges_names.size(); i++) {
                sort_edge_avg_dist_highest_value.push_back({io.edge_dist_num[i], i});
            }
            // 从高到低排
            std::sort(sort_edge_avg_dist_highest_value.begin(),
                      sort_edge_avg_dist_highest_value.end());

            for (int i = 0; i < io.edges_names.size(); i++) {
                int edge_idx = sort_edge_avg_dist_highest_value[i].second;
                // 选取排序存储slot
                std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
                // 边缘节点能被分配到的客户端节点
                auto& clis = io.edge_dist_clients[edge_idx];
                // 排序每个边缘节点能够分配到的客户端的需求总值
                for (int j = 0; j < data_dm_rowstore.size(); j++) {
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[j];
                    int sumrow = 0;
                    for (auto cli : clis) {
                        auto& cli_streams = row[cli];
                        for (auto& cli_stream : cli_streams) {
                            sumrow += cli_stream.first;
                        }
                    }
                    sort_require[j] = {sumrow, j};
                }
                // 也是从高到低排
                std::sort(sort_require.begin(), sort_require.end(),
                          std::greater<std::pair<int, int>>());

                int five_count = 0;   // 边缘节点分配次数的计数
                int require_idx = 0;  // 需求排序后遍历的idx

                while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                    int time = sort_require[require_idx].second;
                    // 获取当前时间点边缘节点剩余的带宽
                    auto& sb_map_ref = sb_map_alltime_in[time];
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[time];
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
                        std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                                  std::greater<std::pair<int, std::string>>());
                        for (auto& cli_stream_dm : row[cli_idx]) {
                            int& cli_dm = cli_stream_dm.first;
                            if (cli_dm == 0)
                                continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                            // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                            // TODO:对节点的流需求进行排序

                            int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                            int dist_v = 0;
                            if (edge_rest_sb >= cli_dm) {
                                dist_v = cli_dm;
                            } else {
                                dist_v = 0;
                            }
                            if (dist_v) {
                                edge_rest_sb -= dist_v;
                                cli_dm -= dist_v;
                                dist_v_acc += dist_v;
                                // res[time][cli_idx][edge_idx].push_back(cli_stream_dm.second);
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
            std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

            for (int i = 0; i < data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = data_dm_rowstore[i];
                int sumrow = 0;
                for (auto cli_streams : row) {
                    for (auto cli_stream : cli_streams) {
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
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

                // 获取edge剩余带宽
                auto& sb_map_ref = sb_map_alltime_in[time];
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
                    std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                    std::sort(cli_streams.begin(), cli_streams.end(),
                              std::greater<std::pair<int, std::string>>());
                    // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                    std::vector<std::tuple<int, int, int>> sort_array;
                    for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                        sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                              io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                              io.qos_map[cli_idx][i]});
                    }
                    // 注意这里是从高到低排，跟之前不一样
                    std::sort(sort_array.begin(), sort_array.end(),
                              [](const std::tuple<int, int, int>& i,
                                 const std::tuple<int, int, int>& j) { return i > j; });

                    // 使用已经超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge从未被超分配过，直接跳过
                        // if (!edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for (auto& cli_stream : cli_streams) {
                            // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if (cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if (edge_rest_sb >= cli_stream.first) {
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
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                        if (edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    }

                    // // 使用未超分配过的edge进行分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                    //     if (edge_exceed[edge_idx]) continue;
                    //     // 获取当前节点剩余带宽
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     for (auto& cli_stream : cli_streams) {
                    //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = 0;
                    //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                    //         if (edge_rest_sb >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first -= dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             edge_record_acm[edge_idx] += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    //     if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    // }
                }

                for (size_t i = 0; i < edge_times.size(); i++) {
                    // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                    edge_times[i] += edge_used[i];
                }
            }
            edge_alloc_in_first = edge_exceed;
        }

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

        // 开始第一轮分配，第一轮分配是面向边缘节点分配
        // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
        std::vector<std::pair<int, int>> sort_edge_avg_dist_highest_value;
        for (size_t i = 0; i < io.edges_names.size(); i++) {
            sort_edge_avg_dist_highest_value.push_back({io.edge_dist_num[i], i});
        }
        // 从高到低排
        std::sort(sort_edge_avg_dist_highest_value.begin(), sort_edge_avg_dist_highest_value.end());

        for (int i = 0; i < io.edges_names.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
                        }
                    }
                }
                if (dist_v_acc != 0) five_count++;
                require_idx++;
            }
        }

        for (int i = 0; i < io.edges_names.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (!edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= second_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
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
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                // // 使用未超分配过的edge进行分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                //     if (edge_exceed[edge_idx]) continue;

                //     // 获取当前节点剩余带宽
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     for (auto& cli_stream : cli_streams) {
                //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = 0;
                //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                //         if (edge_rest_sb >= cli_stream.first) {
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first -= dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             edge_record_acm[edge_idx] += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //             res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                //         }
                //     }
                //     // if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                // }
            }

            for (size_t i = 0; i < edge_times.size(); i++) {
                // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                edge_times[i] += edge_used[i];
            }
        }
        return;
    }

    // brute_force3_with_basecost_dist训练赛分数538898
    // brute_force3的基础上添加对所有节点base cost填满
    void brute_force3_with_basecost_dist() {
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));

        // 进行两次分配，节点在第一次的分配中被分配过
        std::vector<bool> edge_alloc_in_first(io.edges_names.size());
        {
            auto sb_map_alltime_in = this->sb_map_alltime;
            auto data_dm_rowstore = io.data_dm_rowstore;
            // 边缘节点分配的时间
            std::vector<int> edge_times(io.edges_names.size(), 0);

            // 百分之五的时间点个数
            int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;

            // 已经超分配到边
            std::vector<bool> edge_exceed(io.edges_names.size(), false);

            // edge节点超分配的最大值
            std::vector<int> edge_max_record(io.edges_names.size(), 0);

            // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
            // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
            std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
                io.edges_names.size(),
                std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));

            // 开始第一轮分配，第一轮分配是面向边缘节点分配
            // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
            std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
            for (size_t i = 0; i < io.edges_names.size(); i++) {
                sort_edge_avg_dist_highest_value.push_back(
                    {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
            }
            // 从高到低排
            std::sort(sort_edge_avg_dist_highest_value.begin(),
                      sort_edge_avg_dist_highest_value.end(),
                      std::greater<std::pair<double, int>>());

            for (int i = 0; i < io.edges_names.size(); i++) {
                int edge_idx = sort_edge_avg_dist_highest_value[i].second;
                // 选取排序存储slot
                std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
                // 边缘节点能被分配到的客户端节点
                auto& clis = io.edge_dist_clients[edge_idx];
                // 排序每个边缘节点能够分配到的客户端的需求总值
                for (int j = 0; j < data_dm_rowstore.size(); j++) {
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[j];
                    int sumrow = 0;
                    for (auto cli : clis) {
                        auto& cli_streams = row[cli];
                        for (auto& cli_stream : cli_streams) {
                            sumrow += cli_stream.first;
                        }
                    }
                    sort_require[j] = {sumrow, j};
                }
                // 也是从高到低排
                std::sort(sort_require.begin(), sort_require.end(),
                          std::greater<std::pair<int, int>>());

                int five_count = 0;   // 边缘节点分配次数的计数
                int require_idx = 0;  // 需求排序后遍历的idx

                while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                    int time = sort_require[require_idx].second;
                    // 获取当前时间点边缘节点剩余的带宽
                    auto& sb_map_ref = sb_map_alltime_in[time];
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[time];
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
                        std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                                  std::greater<std::pair<int, std::string>>());
                        for (auto& cli_stream_dm : row[cli_idx]) {
                            int& cli_dm = cli_stream_dm.first;
                            if (cli_dm == 0)
                                continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                            // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                            // TODO:对节点的流需求进行排序

                            int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                            int dist_v = 0;
                            if (edge_rest_sb >= cli_dm) {
                                dist_v = cli_dm;
                            } else {
                                dist_v = 0;
                            }
                            if (dist_v) {
                                edge_rest_sb -= dist_v;
                                cli_dm -= dist_v;
                                dist_v_acc += dist_v;
                                // res[time][cli_idx][edge_idx].push_back(cli_stream_dm.second);
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
            std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

            for (int i = 0; i < data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = data_dm_rowstore[i];
                int sumrow = 0;
                for (auto cli_streams : row) {
                    for (auto cli_stream : cli_streams) {
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
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

                // 获取edge剩余带宽
                auto& sb_map_ref = sb_map_alltime_in[time];
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
                    std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                    // !性能相关，可删除这个排序
                    std::sort(cli_streams.begin(), cli_streams.end(),
                              std::greater<std::pair<int, std::string>>());
                    // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                    std::vector<std::tuple<int, int, int>> sort_array;
                    for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                        sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                              io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                              io.qos_map[cli_idx][i]});
                    }
                    // 注意这里是从高到低排，跟之前不一样
                    std::sort(sort_array.begin(), sort_array.end(),
                              [](const std::tuple<int, int, int>& i,
                                 const std::tuple<int, int, int>& j) { return i > j; });

                    // 先对所有节点进行不超过base_cost的分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                        for (auto& cli_stream : cli_streams) {
                            if (cli_stream.first == 0) continue;
                            int dist_v = io.base_cost - cost_now;
                            if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                                // 分配值为客户端需求
                                dist_v = cli_stream.first;
                                // 减去边缘节点剩余带宽
                                edge_rest_sb -= dist_v;
                                // 客户节点当前流的需求置0
                                cli_stream.first = cli_stream.first - dist_v;
                                // 边缘节点当前时间点已分配的值
                                // 暂时不进行累积，否则会影响到后面的edge_exceed
                                // edge_record_acm[edge_idx] += dist_v;
                                cost_now += dist_v;
                                // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                                edge_max_record[edge_idx] =
                                    std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                    }
                    // 使用已经超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge从未被超分配过，直接跳过
                        // if (!edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for (auto& cli_stream : cli_streams) {
                            // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if (cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if (edge_rest_sb >= cli_stream.first) {
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
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                        if (edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    }

                    // // 使用未超分配过的edge进行分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                    //     if (edge_exceed[edge_idx]) continue;
                    //     // 获取当前节点剩余带宽
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     for (auto& cli_stream : cli_streams) {
                    //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = 0;
                    //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                    //         if (edge_rest_sb >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first -= dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             edge_record_acm[edge_idx] += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    //     if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    // }
                }

                for (size_t i = 0; i < edge_times.size(); i++) {
                    // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                    edge_times[i] += edge_used[i];
                }
            }
            edge_alloc_in_first = edge_exceed;
        }

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
                        }
                    }
                }
                if (dist_v_acc != 0) five_count++;
                require_idx++;
            }
        }

        for (int i = 0; i < io.edges_names.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (!edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= second_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
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
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                // !性能相关，可删除这个排序
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 先对所有节点进行不超过base_cost的分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = io.base_cost - cost_now;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            // 暂时不进行累积，否则会影响到后面的edge_exceed
                            // edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                // // 使用未超分配过的edge进行分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                //     if (edge_exceed[edge_idx]) continue;

                //     // 获取当前节点剩余带宽
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     for (auto& cli_stream : cli_streams) {
                //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = 0;
                //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                //         if (edge_rest_sb >= cli_stream.first) {
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first -= dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             edge_record_acm[edge_idx] += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //             res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                //         }
                //     }
                //     // if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                // }
            }

            for (size_t i = 0; i < edge_times.size(); i++) {
                // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                edge_times[i] += edge_used[i];
            }
        }
        return;
    }

    void average_distribute() {
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
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
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                std::sort(cli_streams.begin(), cli_streams.end());

                // // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 先对所有节点进行不超过base_cost的分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = io.base_cost - cost_now;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                std::vector<std::pair<int, int>> sort_rest_to_maxrecord_edge(
                    io.qos_map[cli_idx].size());
                for (int i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_rest_to_maxrecord_edge[i] = {edge_max_record[io.qos_map[cli_idx][i]] -
                                                          edge_record_acm[io.qos_map[cli_idx][i]],
                                                      io.qos_map[cli_idx][i]};
                }
                std::sort(sort_rest_to_maxrecord_edge.begin(), sort_rest_to_maxrecord_edge.end(),
                          std::greater<std::pair<int, int>>());

                // 先对最大已分配值进行分配，分配不超过节点的已分配值
                for (size_t i = 0; i < sort_rest_to_maxrecord_edge.size(); i++) {
                    int edge_idx = sort_rest_to_maxrecord_edge[i].second;
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_stream.first &&
                            (edge_max_record[edge_idx] - edge_record_acm[edge_idx]) >=
                                cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                std::vector<std::pair<int, int>> sort_maxrecord_edge(io.qos_map[cli_idx].size());
                for (int i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_maxrecord_edge[i] = {edge_max_record[io.qos_map[cli_idx][i]],
                                              io.qos_map[cli_idx][i]};
                }
                // 从小到大排序
                std::sort(sort_maxrecord_edge.begin(), sort_maxrecord_edge.end());
                int sort_edge_idx = 0;
                for (auto& cli_stream : cli_streams) {
                    if (cli_stream.first == 0) continue;
                    while (true) {
                        int edge_idx = sort_maxrecord_edge[sort_edge_idx].second;
                        sort_edge_idx = (sort_edge_idx + 1) % sort_maxrecord_edge.size();
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                            break;
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

    // brute_force3_with_basecost_dist训练赛分数538898
    // brute_force3的基础上添加对所有节点base cost填满
    void brute_force3_with_more_basecost_dist2(int avg_cost) {
        this->calculate_each_edge_avg_upper_bound(avg_cost);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));

        // 进行两次分配，节点在第一次的分配中被分配过
        std::vector<bool> edge_alloc_in_first(io.edges_names.size());
        {
            auto sb_map_alltime_in = this->sb_map_alltime;
            auto data_dm_rowstore = io.data_dm_rowstore;
            // 边缘节点分配的时间
            std::vector<int> edge_times(io.edges_names.size(), 0);

            // 百分之五的时间点个数
            int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;

            // 已经超分配到边
            std::vector<bool> edge_exceed(io.edges_names.size(), false);

            // edge节点超分配的最大值
            std::vector<int> edge_max_record(io.edges_names.size(), 0);

            // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
            // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
            std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
                io.edges_names.size(),
                std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));

            // 开始第一轮分配，第一轮分配是面向边缘节点分配
            // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
            std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
            for (size_t i = 0; i < io.edges_names.size(); i++) {
                sort_edge_avg_dist_highest_value.push_back(
                    {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
            }
            // 从高到低排
            std::sort(sort_edge_avg_dist_highest_value.begin(),
                      sort_edge_avg_dist_highest_value.end(),
                      std::greater<std::pair<double, int>>());

            for (int i = 0; i < io.edges_names.size(); i++) {
                int edge_idx = sort_edge_avg_dist_highest_value[i].second;
                // 选取排序存储slot
                std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
                // 边缘节点能被分配到的客户端节点
                auto& clis = io.edge_dist_clients[edge_idx];
                // 排序每个边缘节点能够分配到的客户端的需求总值
                for (int j = 0; j < data_dm_rowstore.size(); j++) {
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[j];
                    int sumrow = 0;
                    for (auto cli : clis) {
                        auto& cli_streams = row[cli];
                        for (auto& cli_stream : cli_streams) {
                            sumrow += cli_stream.first;
                        }
                    }
                    sort_require[j] = {sumrow, j};
                }
                // 也是从高到低排
                std::sort(sort_require.begin(), sort_require.end(),
                          std::greater<std::pair<int, int>>());

                int five_count = 0;   // 边缘节点分配次数的计数
                int require_idx = 0;  // 需求排序后遍历的idx

                while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                    int time = sort_require[require_idx].second;
                    // 获取当前时间点边缘节点剩余的带宽
                    auto& sb_map_ref = sb_map_alltime_in[time];
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[time];
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
                        std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                                  std::greater<std::pair<int, std::string>>());
                        for (auto& cli_stream_dm : row[cli_idx]) {
                            int& cli_dm = cli_stream_dm.first;
                            if (cli_dm == 0)
                                continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                            // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                            // TODO:对节点的流需求进行排序

                            int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                            int dist_v = 0;
                            if (edge_rest_sb >= cli_dm) {
                                dist_v = cli_dm;
                            } else {
                                dist_v = 0;
                            }
                            if (dist_v) {
                                edge_rest_sb -= dist_v;
                                cli_dm -= dist_v;
                                dist_v_acc += dist_v;
                                // res[time][cli_idx][edge_idx].push_back(cli_stream_dm.second);
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
            std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

            for (int i = 0; i < data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = data_dm_rowstore[i];
                int sumrow = 0;
                for (auto cli_streams : row) {
                    for (auto cli_stream : cli_streams) {
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
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

                // 获取edge剩余带宽
                auto& sb_map_ref = sb_map_alltime_in[time];
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
                    std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                    // !性能相关，可删除这个排序
                    std::sort(cli_streams.begin(), cli_streams.end(),
                              std::greater<std::pair<int, std::string>>());
                    // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                    std::vector<std::tuple<int, int, int>> sort_array;
                    for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                        sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                              io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                              io.qos_map[cli_idx][i]});
                    }
                    // 注意这里是从高到低排，跟之前不一样
                    std::sort(sort_array.begin(), sort_array.end(),
                              [](const std::tuple<int, int, int>& i,
                                 const std::tuple<int, int, int>& j) { return i > j; });

                    // 先对所有节点进行不超过base_cost的分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                        for (auto& cli_stream : cli_streams) {
                            if (cli_stream.first == 0) continue;
                            int dist_v = edges_avg_limit[edge_idx] - cost_now;
                            if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                                // 分配值为客户端需求
                                dist_v = cli_stream.first;
                                // 减去边缘节点剩余带宽
                                edge_rest_sb -= dist_v;
                                // 客户节点当前流的需求置0
                                cli_stream.first = cli_stream.first - dist_v;
                                // 边缘节点当前时间点已分配的值
                                // 暂时不进行累积，否则会影响到后面的edge_exceed
                                // edge_record_acm[edge_idx] += dist_v;
                                cost_now += dist_v;
                                // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                                edge_max_record[edge_idx] =
                                    std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                    }
                    // // 再对所有节点进行不超过avg limit的分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    //     for (auto& cli_stream : cli_streams) {
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = edges_avg_limit[edge_idx] - cost_now;
                    //         if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first = cli_stream.first - dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             // 暂时不进行累积，否则会影响到后面的edge_exceed
                    //             // edge_record_acm[edge_idx] += dist_v;
                    //             cost_now += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    // }
                    // 使用已经超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge从未被超分配过，直接跳过
                        // if (!edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for (auto& cli_stream : cli_streams) {
                            // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if (cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if (edge_rest_sb >= cli_stream.first) {
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
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                        if (edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    }

                    // // 使用未超分配过的edge进行分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                    //     if (edge_exceed[edge_idx]) continue;
                    //     // 获取当前节点剩余带宽
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     for (auto& cli_stream : cli_streams) {
                    //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = 0;
                    //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                    //         if (edge_rest_sb >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first -= dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             edge_record_acm[edge_idx] += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    //     if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    // }
                }

                for (size_t i = 0; i < edge_times.size(); i++) {
                    // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                    edge_times[i] += edge_used[i];
                }
            }
            edge_alloc_in_first = edge_exceed;
        }

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
                        }
                    }
                }
                if (dist_v_acc != 0) five_count++;
                require_idx++;
            }
        }

        for (int i = 0; i < io.edges_names.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (!edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= second_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
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
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                // !性能相关，可删除这个排序
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 先对所有节点进行不超过base_cost的分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = edges_avg_limit[edge_idx] - cost_now;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            // 暂时不进行累积，否则会影响到后面的edge_exceed
                            // edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                // // 再对所有节点进行不超过avg limit的分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                //     for (auto& cli_stream : cli_streams) {
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = edges_avg_limit[edge_idx] - cost_now;
                //         if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                //             res[time][cli_idx][edge_idx].push_back(cli_stream);
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first = cli_stream.first - dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             // 暂时不进行累积，否则会影响到后面的edge_exceed
                //             // edge_record_acm[edge_idx] += dist_v;
                //             cost_now += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //         }
                //     }
                // }

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                // // 使用未超分配过的edge进行分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                //     if (edge_exceed[edge_idx]) continue;

                //     // 获取当前节点剩余带宽
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     for (auto& cli_stream : cli_streams) {
                //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = 0;
                //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                //         if (edge_rest_sb >= cli_stream.first) {
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first -= dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             edge_record_acm[edge_idx] += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //             res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                //         }
                //     }
                //     // if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                // }
            }

            for (size_t i = 0; i < edge_times.size(); i++) {
                // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                edge_times[i] += edge_used[i];
            }
        }
        return;
    }

    // brute_force3_with_more_basecost_dist训练赛分数510747
    // brute_force3的基础上添加系数
    void brute_force3_with_more_basecost_dist(int avg_cost) {
        this->calculate_each_edge_avg_upper_bound(avg_cost);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));

        // 进行两次分配，节点在第一次的分配中被分配过
        std::vector<bool> edge_alloc_in_first(io.edges_names.size());
        {
            auto sb_map_alltime_in = this->sb_map_alltime;
            auto data_dm_rowstore = io.data_dm_rowstore;
            // 边缘节点分配的时间
            std::vector<int> edge_times(io.edges_names.size(), 0);

            // 百分之五的时间点个数
            int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;

            // 已经超分配到边
            std::vector<bool> edge_exceed(io.edges_names.size(), false);

            // edge节点超分配的最大值
            std::vector<int> edge_max_record(io.edges_names.size(), 0);

            // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
            // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
            std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
                io.edges_names.size(),
                std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));

            // 开始第一轮分配，第一轮分配是面向边缘节点分配
            // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
            std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
            for (size_t i = 0; i < io.edges_names.size(); i++) {
                sort_edge_avg_dist_highest_value.push_back(
                    {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
            }
            // 从高到低排
            std::sort(sort_edge_avg_dist_highest_value.begin(),
                      sort_edge_avg_dist_highest_value.end(),
                      std::greater<std::pair<double, int>>());

            for (int i = 0; i < io.edges_names.size(); i++) {
                int edge_idx = sort_edge_avg_dist_highest_value[i].second;
                // 选取排序存储slot
                std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
                // 边缘节点能被分配到的客户端节点
                auto& clis = io.edge_dist_clients[edge_idx];
                // 排序每个边缘节点能够分配到的客户端的需求总值
                for (int j = 0; j < data_dm_rowstore.size(); j++) {
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[j];
                    int sumrow = 0;
                    for (auto cli : clis) {
                        auto& cli_streams = row[cli];
                        for (auto& cli_stream : cli_streams) {
                            sumrow += cli_stream.first;
                        }
                    }
                    sort_require[j] = {sumrow, j};
                }
                // 也是从高到低排
                std::sort(sort_require.begin(), sort_require.end(),
                          std::greater<std::pair<int, int>>());

                int five_count = 0;   // 边缘节点分配次数的计数
                int require_idx = 0;  // 需求排序后遍历的idx

                while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                    int time = sort_require[require_idx].second;
                    // 获取当前时间点边缘节点剩余的带宽
                    auto& sb_map_ref = sb_map_alltime_in[time];
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[time];
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
                        std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                                  std::greater<std::pair<int, std::string>>());
                        for (auto& cli_stream_dm : row[cli_idx]) {
                            int& cli_dm = cli_stream_dm.first;
                            if (cli_dm == 0)
                                continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                            // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                            // TODO:对节点的流需求进行排序

                            int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                            int dist_v = 0;
                            if (edge_rest_sb >= cli_dm) {
                                dist_v = cli_dm;
                            } else {
                                dist_v = 0;
                            }
                            if (dist_v) {
                                edge_rest_sb -= dist_v;
                                cli_dm -= dist_v;
                                dist_v_acc += dist_v;
                                // res[time][cli_idx][edge_idx].push_back(cli_stream_dm.second);
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
            std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

            for (int i = 0; i < data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = data_dm_rowstore[i];
                int sumrow = 0;
                for (auto cli_streams : row) {
                    for (auto cli_stream : cli_streams) {
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
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

                // 获取edge剩余带宽
                auto& sb_map_ref = sb_map_alltime_in[time];
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
                    std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                    // !性能相关，可删除这个排序
                    std::sort(cli_streams.begin(), cli_streams.end(),
                              std::greater<std::pair<int, std::string>>());
                    // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                    std::vector<std::tuple<int, int, int>> sort_array;
                    for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                        sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                              io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                              io.qos_map[cli_idx][i]});
                    }
                    // 注意这里是从高到低排，跟之前不一样
                    std::sort(sort_array.begin(), sort_array.end(),
                              [](const std::tuple<int, int, int>& i,
                                 const std::tuple<int, int, int>& j) { return i > j; });

                    // 先对所有节点进行不超过base_cost的分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                        for (auto& cli_stream : cli_streams) {
                            if (cli_stream.first == 0) continue;
                            int dist_v = edges_avg_limit[edge_idx] - cost_now;
                            if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                                // 分配值为客户端需求
                                dist_v = cli_stream.first;
                                // 减去边缘节点剩余带宽
                                edge_rest_sb -= dist_v;
                                // 客户节点当前流的需求置0
                                cli_stream.first = cli_stream.first - dist_v;
                                // 边缘节点当前时间点已分配的值
                                // 暂时不进行累积，否则会影响到后面的edge_exceed
                                // edge_record_acm[edge_idx] += dist_v;
                                cost_now += dist_v;
                                // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                                edge_max_record[edge_idx] =
                                    std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                    }
                    // // 再对所有节点进行不超过avg limit的分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    //     for (auto& cli_stream : cli_streams) {
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = edges_avg_limit[edge_idx] - cost_now;
                    //         if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first = cli_stream.first - dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             // 暂时不进行累积，否则会影响到后面的edge_exceed
                    //             // edge_record_acm[edge_idx] += dist_v;
                    //             cost_now += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    // }
                    // 使用已经超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge从未被超分配过，直接跳过
                        // if (!edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for (auto& cli_stream : cli_streams) {
                            // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if (cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if (edge_rest_sb >= cli_stream.first) {
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
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                        if (edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    }

                    // // 使用未超分配过的edge进行分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                    //     if (edge_exceed[edge_idx]) continue;
                    //     // 获取当前节点剩余带宽
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     for (auto& cli_stream : cli_streams) {
                    //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = 0;
                    //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                    //         if (edge_rest_sb >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first -= dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             edge_record_acm[edge_idx] += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    //     if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    // }
                }

                for (size_t i = 0; i < edge_times.size(); i++) {
                    // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                    edge_times[i] += edge_used[i];
                }
            }
            edge_alloc_in_first = edge_exceed;
        }

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
                        }
                    }
                }
                if (dist_v_acc != 0) five_count++;
                require_idx++;
            }
        }

        for (int i = 0; i < io.edges_names.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (!edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= second_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
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
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                // !性能相关，可删除这个排序
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 先对所有节点进行不超过base_cost的分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = edges_avg_limit[edge_idx] - cost_now;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            // 暂时不进行累积，否则会影响到后面的edge_exceed
                            // edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                // // 再对所有节点进行不超过avg limit的分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                //     for (auto& cli_stream : cli_streams) {
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = edges_avg_limit[edge_idx] - cost_now;
                //         if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                //             res[time][cli_idx][edge_idx].push_back(cli_stream);
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first = cli_stream.first - dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             // 暂时不进行累积，否则会影响到后面的edge_exceed
                //             // edge_record_acm[edge_idx] += dist_v;
                //             cost_now += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //         }
                //     }
                // }

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                // // 使用未超分配过的edge进行分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                //     if (edge_exceed[edge_idx]) continue;

                //     // 获取当前节点剩余带宽
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     for (auto& cli_stream : cli_streams) {
                //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = 0;
                //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                //         if (edge_rest_sb >= cli_stream.first) {
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first -= dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             edge_record_acm[edge_idx] += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //             res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                //         }
                //     }
                //     // if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                // }
            }

            for (size_t i = 0; i < edge_times.size(); i++) {
                // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                edge_times[i] += edge_used[i];
            }
        }
        return;
    }

    // brute_force3_with_basecost_dist训练赛分数538898
    // brute_force3的基础上添加对所有节点base cost填满
    void brute_force3_with_basecost_and_more_local_avg_dist(int avg_cost) {
        this->calculate_each_edge_avg_upper_bound(avg_cost);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));

        // 进行两次分配，节点在第一次的分配中被分配过
        std::vector<bool> edge_alloc_in_first(io.edges_names.size());
        int max_first_alloc_idx = 0;
        {
            auto sb_map_alltime_in = this->sb_map_alltime;
            auto data_dm_rowstore = io.data_dm_rowstore;
            // 边缘节点分配的时间
            std::vector<int> edge_times(io.edges_names.size(), 0);

            // 百分之五的时间点个数
            int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;

            // 已经超分配到边
            std::vector<bool> edge_exceed(io.edges_names.size(), false);

            // edge节点超分配的最大值
            std::vector<int> edge_max_record(io.edges_names.size(), 0);

            // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
            // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
            std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
                io.edges_names.size(),
                std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));

            // 开始第一轮分配，第一轮分配是面向边缘节点分配
            // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
            std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
            for (size_t i = 0; i < io.edges_names.size(); i++) {
                sort_edge_avg_dist_highest_value.push_back(
                    {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
            }
            // 从高到低排
            std::sort(sort_edge_avg_dist_highest_value.begin(),
                      sort_edge_avg_dist_highest_value.end(),
                      std::greater<std::pair<double, int>>());

            for (int i = 0; i < io.edges_names.size(); i++) {
                int edge_idx = sort_edge_avg_dist_highest_value[i].second;
                // 选取排序存储slot
                std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
                // 边缘节点能被分配到的客户端节点
                auto& clis = io.edge_dist_clients[edge_idx];
                // 排序每个边缘节点能够分配到的客户端的需求总值
                for (int j = 0; j < data_dm_rowstore.size(); j++) {
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[j];
                    int sumrow = 0;
                    for (auto cli : clis) {
                        auto& cli_streams = row[cli];
                        for (auto& cli_stream : cli_streams) {
                            sumrow += cli_stream.first;
                        }
                    }
                    sort_require[j] = {sumrow, j};
                }
                // 也是从高到低排
                std::sort(sort_require.begin(), sort_require.end(),
                          std::greater<std::pair<int, int>>());

                int five_count = 0;   // 边缘节点分配次数的计数
                int require_idx = 0;  // 需求排序后遍历的idx

                while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                    int time = sort_require[require_idx].second;
                    // 获取当前时间点边缘节点剩余的带宽
                    auto& sb_map_ref = sb_map_alltime_in[time];
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[time];
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
                        std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                                  std::greater<std::pair<int, std::string>>());
                        for (auto& cli_stream_dm : row[cli_idx]) {
                            int& cli_dm = cli_stream_dm.first;
                            if (cli_dm == 0)
                                continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                            // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                            // TODO:对节点的流需求进行排序

                            int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                            int dist_v = 0;
                            if (edge_rest_sb >= cli_dm) {
                                dist_v = cli_dm;
                            } else {
                                dist_v = 0;
                            }
                            if (dist_v) {
                                edge_rest_sb -= dist_v;
                                cli_dm -= dist_v;
                                dist_v_acc += dist_v;
                                // res[time][cli_idx][edge_idx].push_back(cli_stream_dm.second);
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
            std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

            for (int i = 0; i < data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = data_dm_rowstore[i];
                int sumrow = 0;
                for (auto cli_streams : row) {
                    for (auto cli_stream : cli_streams) {
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
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

                // 获取edge剩余带宽
                auto& sb_map_ref = sb_map_alltime_in[time];
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
                    std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                    // !性能相关，可删除这个排序
                    std::sort(cli_streams.begin(), cli_streams.end(),
                              std::greater<std::pair<int, std::string>>());
                    // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                    std::vector<std::tuple<int, int, int>> sort_array;
                    for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                        sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                              io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                              io.qos_map[cli_idx][i]});
                    }
                    // 注意这里是从高到低排，跟之前不一样
                    std::sort(sort_array.begin(), sort_array.end(),
                              [](const std::tuple<int, int, int>& i,
                                 const std::tuple<int, int, int>& j) { return i > j; });

                    // 先对所有节点进行不超过base_cost的分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                        for (auto& cli_stream : cli_streams) {
                            if (cli_stream.first == 0) continue;
                            int dist_v = io.base_cost - cost_now;
                            if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                                // 分配值为客户端需求
                                dist_v = cli_stream.first;
                                // 减去边缘节点剩余带宽
                                edge_rest_sb -= dist_v;
                                // 客户节点当前流的需求置0
                                cli_stream.first = cli_stream.first - dist_v;
                                // 边缘节点当前时间点已分配的值
                                // 暂时不进行累积，否则会影响到后面的edge_exceed
                                // edge_record_acm[edge_idx] += dist_v;
                                cost_now += dist_v;
                                // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                                edge_max_record[edge_idx] =
                                    std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                    }
                    // // 再对所有节点进行不超过avg limit的分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    //     for (auto& cli_stream : cli_streams) {
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = edges_avg_limit[edge_idx] - cost_now;
                    //         if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first = cli_stream.first - dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             // 暂时不进行累积，否则会影响到后面的edge_exceed
                    //             // edge_record_acm[edge_idx] += dist_v;
                    //             cost_now += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    // }
                    // 使用已经超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge从未被超分配过，直接跳过
                        // if (!edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for (auto& cli_stream : cli_streams) {
                            // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if (cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if (edge_rest_sb >= cli_stream.first) {
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
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                        if (edge_record_acm[edge_idx] > 0) {
                            edge_exceed[edge_idx] = true;
                            max_first_alloc_idx = std::max(max_first_alloc_idx, int(i));
                        }
                    }

                    // // 使用未超分配过的edge进行分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                    //     if (edge_exceed[edge_idx]) continue;
                    //     // 获取当前节点剩余带宽
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     for (auto& cli_stream : cli_streams) {
                    //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = 0;
                    //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                    //         if (edge_rest_sb >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first -= dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             edge_record_acm[edge_idx] += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    //     if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    // }
                }

                for (size_t i = 0; i < edge_times.size(); i++) {
                    // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                    edge_times[i] += edge_used[i];
                }
            }
            edge_alloc_in_first = edge_exceed;
        }

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
                        }
                    }
                }
                if (dist_v_acc != 0) five_count++;
                require_idx++;
            }
        }

        for (int i = 0; i < io.edges_names.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (!edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= second_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
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
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                // !性能相关，可删除这个排序
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 先对所有节点进行不超过base_cost的分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = io.base_cost - cost_now;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            // 暂时不进行累积，否则会影响到后面的edge_exceed
                            // edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                // 再对所有节点进行不超过avg limit的分配
                for (size_t i = 0; i < max_first_alloc_idx * 4 && i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = edges_avg_limit[edge_idx] - cost_now;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            // 暂时不进行累积，否则会影响到后面的edge_exceed
                            // edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                // // 使用未超分配过的edge进行分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                //     if (edge_exceed[edge_idx]) continue;

                //     // 获取当前节点剩余带宽
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     for (auto& cli_stream : cli_streams) {
                //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = 0;
                //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                //         if (edge_rest_sb >= cli_stream.first) {
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first -= dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             edge_record_acm[edge_idx] += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //             res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                //         }
                //     }
                //     // if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                // }
            }

            for (size_t i = 0; i < edge_times.size(); i++) {
                // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                edge_times[i] += edge_used[i];
            }
        }
        return;
    }

    void brute_force3_with_coeffiicient_avg_dist_with_calculate_first_score(double avg_coefficient) {
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));
        // 进行两次分配，节点在第一次的分配中被分配过
        std::vector<bool> edge_alloc_in_first(io.edges_names.size());
        double rest_dist = 0;
        {
            auto sb_map_alltime_in = this->sb_map_alltime;
            auto data_dm_rowstore = io.data_dm_rowstore;
            // 边缘节点分配的时间
            std::vector<int> edge_times(io.edges_names.size(), 0);

            // 百分之五的时间点个数
            int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;

            // 已经超分配到边
            std::vector<bool> edge_exceed(io.edges_names.size(), false);

            // edge节点超分配的最大值
            std::vector<int> edge_max_record(io.edges_names.size(), 0);

            // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
            // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
            std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
                io.edges_names.size(),
                std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));

            // 开始第一轮分配，第一轮分配是面向边缘节点分配
            // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
            std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
            for (size_t i = 0; i < io.edges_names.size(); i++) {
                sort_edge_avg_dist_highest_value.push_back(
                    {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
            }
            // 从高到低排
            std::sort(sort_edge_avg_dist_highest_value.begin(),
                      sort_edge_avg_dist_highest_value.end(),
                      std::greater<std::pair<double, int>>());

            for (int i = 0; i < io.edges_names.size(); i++) {
                int edge_idx = sort_edge_avg_dist_highest_value[i].second;
                // 如果第一次尝试二轮分配时分配过，则跳过
                // if (edge_alloc_in_first[edge_idx]) continue;
                // 选取排序存储slot
                std::vector<std::tuple<int, int, int>> sort_require(data_dm_rowstore.size());
                // 边缘节点能被分配到的客户端节点
                auto& clis = io.edge_dist_clients[edge_idx];
                // 排序每个边缘节点能够分配到的客户端的需求总值
                for (int j = 0; j < data_dm_rowstore.size(); j++) {
                    std::vector<std::vector<std::pair<int, std::string>>>& row = data_dm_rowstore[j];
                    int sumrow = 0;
                    int max_stream_sum = 0;
                    for (auto cli : clis) {
                        auto& cli_streams = row[cli];
                        int max_stream = 0;
                        for (auto& cli_stream : cli_streams) {
                            sumrow += cli_stream.first;
                            max_stream = std::max(cli_stream.first, max_stream);
                        }
                        max_stream_sum += max_stream;
                    }
                    sort_require[j] = {sumrow, max_stream_sum, j};
                    // sort_require[j] = {max_stream_sum, sumrow, j};
                }
                // 也是从高到低排
                std::sort(sort_require.begin(), sort_require.end(),
                        [](const std::tuple<int, int, int>& i,
                                const std::tuple<int, int, int>& j) { return i > j; });

                int five_count = 0;   // 边缘节点分配次数的计数
                int require_idx = 0;  // 需求排序后遍历的idx
                while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                    // int time = sort_require[require_idx].second;
                    int time = std::get<2>(sort_require[require_idx]);
                    // 获取当前时间点边缘节点剩余的带宽
                    auto& sb_map_ref = sb_map_alltime_in[time];
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[time];
                    // 计算当前edge节点在当前时间点分配了多少
                    int dist_v_acc = 0;

                    std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
                    for(int idx=0; idx < clis.size(); idx++) {
                        int cli_idx = clis[idx];
                        auto& streams = row[cli_idx];
                        for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                            sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                        }
                    }
                    std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                                const std::tuple<int, int, int>& j) { return i > j; });

                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& sort_stream: sort_cli_streams) {
                        int stream = std::get<0>(sort_stream);
                        if(stream == 0) break;
                        int stream_idx = std::get<1>(sort_stream);
                        int cli_idx = std::get<2>(sort_stream);
                        if (edge_rest_sb >= stream) {
                            // res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                            edge_rest_sb -= stream;
                            row[cli_idx][stream_idx].first = 0;
                            dist_v_acc += stream;
                        }
                    }
                    if (dist_v_acc != 0)  {
                        five_count++;
                        // edge_5_percent_time[edge_idx].push_back(time);
                    }
                    require_idx++;
                }
            }
            // 以上5%分配完

            // 开始第二轮分配，第二轮分配是面向客户节点分配
            // 对所有客户节点总需求量排序，以此选择时间点
            std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

            for (int i = 0; i < data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = data_dm_rowstore[i];
                int sumrow = 0;
                for (auto cli_streams : row) {
                    for (auto cli_stream : cli_streams) {
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
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

                // 获取edge剩余带宽
                auto& sb_map_ref = sb_map_alltime_in[time];
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
                    std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                    // !性能相关，可删除这个排序
                    std::sort(cli_streams.begin(), cli_streams.end(),
                              std::greater<std::pair<int, std::string>>());
                    // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                    std::vector<std::tuple<int, int, int>> sort_array;
                    for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                        sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                              io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                              io.qos_map[cli_idx][i]});
                    }
                    // 注意这里是从高到低排，跟之前不一样
                    std::sort(sort_array.begin(), sort_array.end(),
                              [](const std::tuple<int, int, int>& i,
                                 const std::tuple<int, int, int>& j) { return i > j; });

                    // 先对所有节点进行不超过base_cost的分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                        for (auto& cli_stream : cli_streams) {
                            if (cli_stream.first == 0) continue;
                            int dist_v = io.base_cost - cost_now;
                            if(dist_v <= 0) break;
                            if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                                // 分配值为客户端需求
                                dist_v = cli_stream.first;
                                // 减去边缘节点剩余带宽
                                edge_rest_sb -= dist_v;
                                // 客户节点当前流的需求置0
                                cli_stream.first = cli_stream.first - dist_v;
                                // 边缘节点当前时间点已分配的值
                                // 暂时不进行累积，否则会影响到后面的edge_exceed
                                // edge_record_acm[edge_idx] += dist_v;
                                cost_now += dist_v;
                                // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                                edge_max_record[edge_idx] =
                                    std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                    }
                    // // 再对所有节点进行不超过avg limit的分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    //     for (auto& cli_stream : cli_streams) {
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = edges_avg_limit[edge_idx] - cost_now;
                    //         if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first = cli_stream.first - dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             // 暂时不进行累积，否则会影响到后面的edge_exceed
                    //             // edge_record_acm[edge_idx] += dist_v;
                    //             cost_now += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    // }
                    // 使用已经超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge从未被超分配过，直接跳过
                        // if (!edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for (auto& cli_stream : cli_streams) {
                            // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if (cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if (edge_rest_sb >= cli_stream.first) {
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
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                        if (edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    }

                    // // 使用未超分配过的edge进行分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                    //     if (edge_exceed[edge_idx]) continue;
                    //     // 获取当前节点剩余带宽
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     for (auto& cli_stream : cli_streams) {
                    //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = 0;
                    //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                    //         if (edge_rest_sb >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first -= dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             edge_record_acm[edge_idx] += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    //     if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    // }
                }

                for (size_t i = 0; i < edge_times.size(); i++) {
                    // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                    edge_times[i] += edge_used[i];
                }
            }
            edge_alloc_in_first = edge_exceed;

            int pos_94 = 0.05 * res.size();
            std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
                io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

            for (int time = 0; time < res.size(); time++) {
                for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                    int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                    sb_map_alltime_in[time][io.edges_names[edge_idx]];
                    edge_sort_score[edge_idx][time] = {edge_dist, time};
                }
            }
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                auto edge_score = edge_sort_score[edge_idx];
                std::sort(edge_score.begin(), edge_score.end(),
                          std::greater<std::pair<int, int>>());
                rest_dist += edge_score[pos_94].first;
            }
        }

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (edge_alloc_in_first[edge_idx]) continue;
            // 选取排序存储slot
            std::vector<std::tuple<int, int, int>> sort_require(io.data_dm_rowstore.size());
            // 边缘节点能被分配到的客户端节点
            auto& clis = io.edge_dist_clients[edge_idx];
            // 排序每个边缘节点能够分配到的客户端的需求总值
            for (int j = 0; j < io.data_dm_rowstore.size(); j++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[j];
                int sumrow = 0;
                int max_stream_sum = 0;
                for (auto cli : clis) {
                    auto& cli_streams = row[cli];
                    int max_stream = 0;
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                        max_stream = std::max(cli_stream.first, max_stream);
                    }
                    max_stream_sum += max_stream;
                }
                sort_require[j] = {sumrow, max_stream_sum, j};
                // sort_require[j] = {max_stream_sum, sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx
            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                // int time = sort_require[require_idx].second;
                int time = std::get<2>(sort_require[require_idx]);
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
                // 计算当前edge节点在当前时间点分配了多少
                int dist_v_acc = 0;

                std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
                for(int idx=0; idx < clis.size(); idx++) {
                    int cli_idx = clis[idx];
                    auto& streams = row[cli_idx];
                    for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                        sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                    }
                }
                std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                for (auto& sort_stream: sort_cli_streams) {
                    int stream = std::get<0>(sort_stream);
                    if(stream == 0) break;
                    int stream_idx = std::get<1>(sort_stream);
                    int cli_idx = std::get<2>(sort_stream);
                    if (edge_rest_sb >= stream) {
                        res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                        edge_rest_sb -= stream;
                        row[cli_idx][stream_idx].first = 0;
                        dist_v_acc += stream;
                    }
                }
                if (dist_v_acc != 0)  {
                    five_count++;
                    // edge_5_percent_time[edge_idx].push_back(time);
                }
                require_idx++;
            }
        }
        // 以上5%分配完
        // !计算avg limit
        this->calculate_each_edge_avg_dist_limit(avg_coefficient);

        // 开始第二轮分配，第二轮分配是面向客户节点分配
        // 对所有客户节点总需求量排序，以此选择时间点
        std::vector<std::pair<int, int>> sort_time(io.data_dm_rowstore.size(), {0, 0});

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            int sumrow = 0;
            for (auto cli_streams : row) {
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                // !性能相关，可删除这个排序
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 先对所有节点进行不超过base_cost的分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = io.base_cost - cost_now;
                        if(dist_v <= 0) break;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            // 暂时不进行累积，否则会影响到后面的edge_exceed
                            // edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                // 再对所有节点进行不超过avg limit的分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = edges_avg_limit[edge_idx] - cost_now;
                        if(dist_v <= 0) break;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            // 暂时不进行累积，否则会影响到后面的edge_exceed
                            // edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                // // 使用未超分配过的edge进行分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                //     if (edge_exceed[edge_idx]) continue;

                //     // 获取当前节点剩余带宽
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     for (auto& cli_stream : cli_streams) {
                //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = 0;
                //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                //         if (edge_rest_sb >= cli_stream.first) {
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first -= dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             edge_record_acm[edge_idx] += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //             res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                //         }
                //     }
                //     // if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                // }
            }

            for (size_t i = 0; i < edge_times.size(); i++) {
                // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                edge_times[i] += edge_used[i];
            }
        }
        return;
    }


    void brute_force3_with_coeffiicient_avg_dist(double avg_coefficient) {
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // // 如果第一次尝试二轮分配时分配过，则跳过
            // if (edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx

            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
                        }
                    }
                }
                if (dist_v_acc != 0) five_count++;
                require_idx++;
            }
        }

        // 以上5%分配完
        // !计算avg limit
        this->calculate_each_edge_avg_dist_limit(avg_coefficient);

        // 开始第二轮分配，第二轮分配是面向客户节点分配
        // 对所有客户节点总需求量排序，以此选择时间点
        std::vector<std::pair<int, int>> sort_time(io.data_dm_rowstore.size(), {0, 0});

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            int sumrow = 0;
            for (auto cli_streams : row) {
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                // !性能相关，可删除这个排序
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 先对所有节点进行不超过base_cost的分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = io.base_cost - cost_now;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            // 暂时不进行累积，否则会影响到后面的edge_exceed
                            // edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                // 再对所有节点进行不超过avg limit的分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = edges_avg_limit[edge_idx] - cost_now;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            // 暂时不进行累积，否则会影响到后面的edge_exceed
                            // edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                // // 使用未超分配过的edge进行分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                //     if (edge_exceed[edge_idx]) continue;

                //     // 获取当前节点剩余带宽
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     for (auto& cli_stream : cli_streams) {
                //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = 0;
                //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                //         if (edge_rest_sb >= cli_stream.first) {
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first -= dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             edge_record_acm[edge_idx] += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //             res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                //         }
                //     }
                //     // if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                // }
            }

            for (size_t i = 0; i < edge_times.size(); i++) {
                // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                edge_times[i] += edge_used[i];
            }
        }
        return;
    }


    // brute_force3_with_more_basecost_dist训练赛分数510747
    // brute_force3的基础上添加系数
    void brute_force4_with_more_basecost_dist(int avg_cost) {
        this->calculate_each_edge_avg_upper_bound(avg_cost);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));
        int highest_time = 5;
        // 进行两次分配，节点在第一次的分配中被分配过
        std::vector<bool> edge_alloc_in_first(io.edges_names.size());
        {
            auto sb_map_alltime_in = this->sb_map_alltime;
            auto data_dm_rowstore = io.data_dm_rowstore;
            // 边缘节点分配的时间
            std::vector<int> edge_times(io.edges_names.size(), 0);

            // 百分之五的时间点个数
            int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;

            // 已经超分配到边
            std::vector<bool> edge_exceed(io.edges_names.size(), false);

            // edge节点超分配的最大值
            std::vector<int> edge_max_record(io.edges_names.size(), 0);

            // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
            // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
            std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
                io.edges_names.size(),
                std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));

            // 开始第一轮分配，第一轮分配是面向边缘节点分配
            // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
            std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
            for (size_t i = 0; i < io.edges_names.size(); i++) {
                sort_edge_avg_dist_highest_value.push_back(
                    {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
            }
            // 从高到低排
            std::sort(sort_edge_avg_dist_highest_value.begin(),
                      sort_edge_avg_dist_highest_value.end(),
                      std::greater<std::pair<double, int>>());

            for (int i = 0; i < io.edges_names.size(); i++) {
                int edge_idx = sort_edge_avg_dist_highest_value[i].second;
                // 选取排序存储slot
                std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
                // 边缘节点能被分配到的客户端节点
                auto& clis = io.edge_dist_clients[edge_idx];
                // 排序每个边缘节点能够分配到的客户端的需求总值
                for (int j = 0; j < data_dm_rowstore.size(); j++) {
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[j];
                    int sumrow = 0;
                    for (auto cli : clis) {
                        auto& cli_streams = row[cli];
                        for (auto& cli_stream : cli_streams) {
                            sumrow += cli_stream.first;
                        }
                    }
                    sort_require[j] = {sumrow, j};
                }
                // 也是从高到低排
                std::sort(sort_require.begin(), sort_require.end(),
                          std::greater<std::pair<int, int>>());

                int five_count = 0;   // 边缘节点分配次数的计数
                int require_idx = 0;  // 需求排序后遍历的idx

                // while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                //     int time = sort_require[require_idx].second;
                //     // 获取当前时间点边缘节点剩余的带宽
                //     auto& sb_map_ref = sb_map_alltime_in[time];
                //     std::vector<std::vector<std::pair<int, std::string>>>& row =
                //         data_dm_rowstore[time];
                //     // 计算当前edge节点在当前时间点分配了多少
                //     int dist_v_acc = 0;

                //     // 根据客户端的连通性排序当前节点可分配的客户端
                //     std::vector<std::pair<int, int>> sort_cli_edge_nums(clis.size());
                //     for (int j = 0; j < clis.size(); j++) {
                //         int cli_edge_nums = io.qos_map[clis[j]].size();
                //         sort_cli_edge_nums[j] = {cli_edge_nums, clis[j]};
                //     }
                //     // 从低到高排序
                //     std::sort(sort_cli_edge_nums.begin(), sort_cli_edge_nums.end());

                //     for (auto& cli : sort_cli_edge_nums) {
                //         int cli_idx = cli.second;
                //         std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                //                   std::greater<std::pair<int, std::string>>());
                //         for (int stream_idx=0; stream_idx<row[cli_idx].size() && stream_idx < highest_time; stream_idx++) {
                //             int& cli_dm = row[cli_idx][stream_idx].first;
                //             if (cli_dm == 0)
                //                 continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                //             // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                //             // TODO:对节点的流需求进行排序

                //             int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //             int dist_v = 0;
                //             if (edge_rest_sb >= cli_dm) {
                //                 dist_v = cli_dm;
                //             } else {
                //                 dist_v = 0;
                //             }
                //             if (dist_v) {
                //                 edge_rest_sb -= dist_v;
                //                 cli_dm -= dist_v;
                //                 dist_v_acc += dist_v;
                //                 // res[time][cli_idx][edge_idx].push_back(cli_stream_dm.second);
                //             }
                //         }
                //     }
                //     if (dist_v_acc != 0) five_count++;
                //     require_idx++;
                // }
                // five_count = 0; 
                // require_idx = 0;
                while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                    int time = sort_require[require_idx].second;
                    // 获取当前时间点边缘节点剩余的带宽
                    auto& sb_map_ref = sb_map_alltime_in[time];
                    std::vector<std::vector<std::pair<int, std::string>>>& row =
                        data_dm_rowstore[time];
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
                        std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                                  std::greater<std::pair<int, std::string>>());
                        for (auto& cli_stream_dm : row[cli_idx]) {
                            int& cli_dm = cli_stream_dm.first;
                            if (cli_dm == 0)
                                continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                            // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                            // TODO:对节点的流需求进行排序

                            int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                            int dist_v = 0;
                            if (edge_rest_sb >= cli_dm) {
                                dist_v = cli_dm;
                            } else {
                                dist_v = 0;
                            }
                            if (dist_v) {
                                edge_rest_sb -= dist_v;
                                cli_dm -= dist_v;
                                dist_v_acc += dist_v;
                                // res[time][cli_idx][edge_idx].push_back(cli_stream_dm.second);
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
            std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

            for (int i = 0; i < data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = data_dm_rowstore[i];
                int sumrow = 0;
                for (auto cli_streams : row) {
                    for (auto cli_stream : cli_streams) {
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
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

                // 获取edge剩余带宽
                auto& sb_map_ref = sb_map_alltime_in[time];
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
                    std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                    // !性能相关，可删除这个排序
                    std::sort(cli_streams.begin(), cli_streams.end(),
                              std::greater<std::pair<int, std::string>>());
                    // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                    std::vector<std::tuple<int, int, int>> sort_array;
                    for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                        sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                              io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                              io.qos_map[cli_idx][i]});
                    }
                    // 注意这里是从高到低排，跟之前不一样
                    std::sort(sort_array.begin(), sort_array.end(),
                              [](const std::tuple<int, int, int>& i,
                                 const std::tuple<int, int, int>& j) { return i > j; });

                    // 先对所有节点进行不超过base_cost的分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                        for (auto& cli_stream : cli_streams) {
                            if (cli_stream.first == 0) continue;
                            int dist_v = edges_avg_limit[edge_idx] - cost_now;
                            if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                                // 分配值为客户端需求
                                dist_v = cli_stream.first;
                                // 减去边缘节点剩余带宽
                                edge_rest_sb -= dist_v;
                                // 客户节点当前流的需求置0
                                cli_stream.first = cli_stream.first - dist_v;
                                // 边缘节点当前时间点已分配的值
                                // 暂时不进行累积，否则会影响到后面的edge_exceed
                                // edge_record_acm[edge_idx] += dist_v;
                                cost_now += dist_v;
                                // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                                edge_max_record[edge_idx] =
                                    std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                    }
                    // // 再对所有节点进行不超过avg limit的分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    //     for (auto& cli_stream : cli_streams) {
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = edges_avg_limit[edge_idx] - cost_now;
                    //         if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first = cli_stream.first - dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             // 暂时不进行累积，否则会影响到后面的edge_exceed
                    //             // edge_record_acm[edge_idx] += dist_v;
                    //             cost_now += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    // }
                    // 使用已经超分配过的edge进行分配
                    for (size_t i = 0; i < sort_array.size(); i++) {
                        int edge_idx = std::get<2>(sort_array[i]);
                        // 如果没有edge从未被超分配过，直接跳过
                        // if (!edge_exceed[edge_idx]) continue;
                        // 获取当前节点剩余带宽
                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        for (auto& cli_stream : cli_streams) {
                            // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                            if (cli_stream.first == 0) continue;
                            int dist_v = 0;
                            // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                            if (edge_rest_sb >= cli_stream.first) {
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
                                // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                            }
                        }
                        if (edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    }

                    // // 使用未超分配过的edge进行分配
                    // for (size_t i = 0; i < sort_array.size(); i++) {
                    //     int edge_idx = std::get<2>(sort_array[i]);
                    //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                    //     if (edge_exceed[edge_idx]) continue;
                    //     // 获取当前节点剩余带宽
                    //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    //     for (auto& cli_stream : cli_streams) {
                    //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                    //         if (cli_stream.first == 0) continue;
                    //         int dist_v = 0;
                    //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                    //         if (edge_rest_sb >= cli_stream.first) {
                    //             // 分配值为客户端需求
                    //             dist_v = cli_stream.first;
                    //             // 减去边缘节点剩余带宽
                    //             edge_rest_sb -= dist_v;
                    //             // 客户节点当前流的需求置0
                    //             cli_stream.first -= dist_v;
                    //             // 边缘节点当前时间点已分配的值
                    //             edge_record_acm[edge_idx] += dist_v;
                    //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                    //             edge_max_record[edge_idx] =
                    //                 std::max(edge_max_record[edge_idx],
                    //                 edge_record_acm[edge_idx]);
                    //             // res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                    //         }
                    //     }
                    //     if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                    // }
                }

                for (size_t i = 0; i < edge_times.size(); i++) {
                    // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                    edge_times[i] += edge_used[i];
                }
            }
            edge_alloc_in_first = edge_exceed;
        }

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            if (edge_alloc_in_first[edge_idx]) continue;
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
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                    }
                }
                sort_require[j] = {sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      std::greater<std::pair<int, int>>());

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx
            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                                std::greater<std::pair<int, std::string>>());
                    for (int stream_idx=0; stream_idx<row[cli_idx].size() && stream_idx < highest_time; stream_idx++) {
                        int& cli_dm = row[cli_idx][stream_idx].first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
                        }
                    }
                }
                if (dist_v_acc != 0) five_count++;
                require_idx++;
            }
            five_count = 0;
            require_idx = 0;
            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                int time = sort_require[require_idx].second;
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
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
                    std::sort(row[cli_idx].begin(), row[cli_idx].end(),
                              std::greater<std::pair<int, std::string>>());
                    for (auto& cli_stream_dm : row[cli_idx]) {
                        int& cli_dm = cli_stream_dm.first;
                        if (cli_dm == 0)
                            continue;  // 跳过需求为0的节点不分配，因为已经分配过了，或者没有需求
                        // 暴力分配，只要节点有足够的带宽就将当前节点的所有流分配了，知道节点带宽不够
                        // TODO:对节点的流需求进行排序

                        int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                        int dist_v = 0;
                        if (edge_rest_sb >= cli_dm) {
                            dist_v = cli_dm;
                        } else {
                            dist_v = 0;
                        }
                        if (dist_v) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream_dm);
                            edge_rest_sb -= dist_v;
                            cli_dm -= dist_v;
                            dist_v_acc += dist_v;
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
                for (auto cli_stream : cli_streams) {
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
            std::vector<std::vector<std::pair<int, std::string>>>& row =
                io.data_dm_rowstore[time];  // 每个时间段客户端需求的带宽

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
                std::vector<std::pair<int, std::string>>& cli_streams = row[cli_idx];
                // !性能相关，可删除这个排序
                std::sort(cli_streams.begin(), cli_streams.end(),
                          std::greater<std::pair<int, std::string>>());
                // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
                std::vector<std::tuple<int, int, int>> sort_array;
                for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
                    sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
                                          io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
                                          io.qos_map[cli_idx][i]});
                }
                // 注意这里是从高到低排，跟之前不一样
                std::sort(sort_array.begin(), sort_array.end(),
                          [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                // 先对所有节点进行不超过base_cost的分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                    for (auto& cli_stream : cli_streams) {
                        if (cli_stream.first == 0) continue;
                        int dist_v = edges_avg_limit[edge_idx] - cost_now;
                        if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
                            // 分配值为客户端需求
                            dist_v = cli_stream.first;
                            // 减去边缘节点剩余带宽
                            edge_rest_sb -= dist_v;
                            // 客户节点当前流的需求置0
                            cli_stream.first = cli_stream.first - dist_v;
                            // 边缘节点当前时间点已分配的值
                            // 暂时不进行累积，否则会影响到后面的edge_exceed
                            // edge_record_acm[edge_idx] += dist_v;
                            cost_now += dist_v;
                            // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                            edge_max_record[edge_idx] =
                                std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                        }
                    }
                }

                // // 再对所有节点进行不超过avg limit的分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     int cost_now = io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb;
                //     for (auto& cli_stream : cli_streams) {
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = edges_avg_limit[edge_idx] - cost_now;
                //         if (edge_rest_sb >= cli_stream.first && dist_v >= cli_stream.first) {
                //             res[time][cli_idx][edge_idx].push_back(cli_stream);
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first = cli_stream.first - dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             // 暂时不进行累积，否则会影响到后面的edge_exceed
                //             // edge_record_acm[edge_idx] += dist_v;
                //             cost_now += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //         }
                //     }
                // }

                // 使用已经超分配过的edge进行分配
                for (size_t i = 0; i < sort_array.size(); i++) {
                    int edge_idx = std::get<2>(sort_array[i]);
                    // 如果没有edge从未被超分配过，直接跳过
                    // if (!edge_exceed[edge_idx]) continue;
                    // 获取当前节点剩余带宽
                    int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                    for (auto& cli_stream : cli_streams) {
                        // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                        if (cli_stream.first == 0) continue;
                        int dist_v = 0;
                        // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                        if (edge_rest_sb >= cli_stream.first) {
                            res[time][cli_idx][edge_idx].push_back(cli_stream);
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
                        }
                    }
                }

                // // 使用未超分配过的edge进行分配
                // for (size_t i = 0; i < sort_array.size(); i++) {
                //     int edge_idx = std::get<2>(sort_array[i]);
                //     // 如果没有edge已经被超分配过，直接跳过，不过此步可能是多次一举
                //     if (edge_exceed[edge_idx]) continue;

                //     // 获取当前节点剩余带宽
                //     int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                //     for (auto& cli_stream : cli_streams) {
                //         // 如果客户端当前流的需求为0，表示已经分配过或无需求，跳过
                //         if (cli_stream.first == 0) continue;
                //         int dist_v = 0;
                //         // 暴力分配，如果边缘节点的剩余带宽足够，直接分配客户端当前流的需求
                //         if (edge_rest_sb >= cli_stream.first) {
                //             // 分配值为客户端需求
                //             dist_v = cli_stream.first;
                //             // 减去边缘节点剩余带宽
                //             edge_rest_sb -= dist_v;
                //             // 客户节点当前流的需求置0
                //             cli_stream.first -= dist_v;
                //             // 边缘节点当前时间点已分配的值
                //             edge_record_acm[edge_idx] += dist_v;
                //             // 更新edge超分配的最大值，这个值当前还没有用到，留作后期用
                //             edge_max_record[edge_idx] =
                //                 std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                //             res[time][cli_idx][edge_idx].push_back(cli_stream.second);
                //         }
                //     }
                //     // if(edge_record_acm[edge_idx] > 0) edge_exceed[edge_idx] = true;
                // }
            }

            for (size_t i = 0; i < edge_times.size(); i++) {
                // 更新边缘节点已经被分配的次数，这个值当前同样没有被用到
                edge_times[i] += edge_used[i];
            }
        }
        return;
    }

    
    // 使用max_stream_sum排序，训练赛分数683472 佳玺数据集分数1170w
    // 使用sumrow排序，训练赛分数602577，佳玺1153w
    void brute_force5() {
        // this->calculate_each_edge_avg_upper_bound(avg_cost);
        edge_5_percent_time.resize(io.edges_names.size());
        edge_94_dist.resize(io.edges_names.size(), 0);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));
        int highest_time = 5;

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        // std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
        //     io.edges_names.size(),
        //     std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            // if (edge_alloc_in_first[edge_idx]) continue;
            // 选取排序存储slot
            std::vector<std::tuple<int, int, int>> sort_require(io.data_dm_rowstore.size());
            // 边缘节点能被分配到的客户端节点
            auto& clis = io.edge_dist_clients[edge_idx];
            // 排序每个边缘节点能够分配到的客户端的需求总值
            for (int j = 0; j < io.data_dm_rowstore.size(); j++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[j];
                int sumrow = 0;
                int max_stream_sum = 0;
                for (auto cli : clis) {
                    auto& cli_streams = row[cli];
                    int max_stream = 0;
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                        max_stream = std::max(cli_stream.first, max_stream);
                    }
                    max_stream_sum += max_stream;
                }
                // sort_require[j] = {sumrow, max_stream, j};
                // sort_require[j] = {max_stream_sum, sumrow, j};
                sort_require[j] = {sumrow, max_stream_sum, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx
            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                // int time = sort_require[require_idx].second;
                int time = std::get<2>(sort_require[require_idx]);
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
                // 计算当前edge节点在当前时间点分配了多少
                int dist_v_acc = 0;

                std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
                for(int idx=0; idx < clis.size(); idx++) {
                    int cli_idx = clis[idx];
                    auto& streams = row[cli_idx];
                    for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                        sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                    }
                }
                std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                for (auto& sort_stream: sort_cli_streams) {
                    int stream = std::get<0>(sort_stream);
                    if(stream == 0) break;
                    int stream_idx = std::get<1>(sort_stream);
                    int cli_idx = std::get<2>(sort_stream);
                    if (edge_rest_sb >= stream) {
                        res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                        edge_rest_sb -= stream;
                        row[cli_idx][stream_idx].first = 0;
                        dist_v_acc += stream;
                    }
                }
                if (dist_v_acc != 0)  {
                    five_count++;
                    edge_5_percent_time[edge_idx].push_back(time);
                }
                require_idx++;
            }
        }
        // 以上5%分配完

        // 开始第二轮分配，第二轮分配是面向客户节点分配
        // 对所有客户节点总需求量排序，以此选择时间点
        std::vector<std::tuple<int, int, int, int>> sort_stream;

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    sort_stream.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx, i});
                }
            }
        }
        // 从高到低排
        std::sort(sort_stream.begin(), sort_stream.end()
                    , 
                    [](const std::tuple<int, int, int, int>& i,
                    const std::tuple<int, int, int, int>& j) { return i > j; }
                    );

        int pos_94 = 0.05 * res.size();
        std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
            io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

        for (int time = 0; time < res.size(); time++) {
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                sb_map_alltime[time][io.edges_names[edge_idx]];
                edge_sort_score[edge_idx][time] = {edge_dist, time};
            }
        }
        double sum = 0;
        for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
            auto edge_score = edge_sort_score[edge_idx];
            std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
            edge_94_dist[edge_idx] = edge_score[pos_94].first;
            
        }
        for (size_t st = 0; st < sort_stream.size(); st++) {
            int time = std::get<3>(sort_stream[st]);
            int cli_idx = std::get<2>(sort_stream[st]);
            int stream_idx = std::get<1>(sort_stream[st]);
            int stream = std::get<0>(sort_stream[st]);
            // if(st < 100) std::cout << st << " " << stream << std::endl;
            if(stream == 0) continue;
            int edge_choose = calculate_best_edge(time, cli_idx, stream);
            
            int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
            auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

            // 添加结果
            res[time][cli_idx][edge_choose].push_back(cli_stream);
            // 分配值为客户端需求
            int dist_v = cli_stream.first;
            // 减去边缘节点剩余带宽
            edge_rest_sb -= dist_v;
            // 客户节点当前流的需求置0
            cli_stream.first = cli_stream.first - dist_v;

            int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
            if(edge_dist_now > edge_94_dist[edge_choose]) {
                bool is_in_5_percent = false;
                for (auto time_5 : edge_5_percent_time[edge_choose]) {
                    if(time == time_5) is_in_5_percent = true;
                }
                if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
            }
            edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            
        }
        return;
    }


    void brute_force5_with_edge_limit(double avg_coefficient) {
        // this->calculate_each_edge_avg_upper_bound(avg_cost);
        edge_5_percent_time.resize(io.edges_names.size());
        edge_94_dist.resize(io.edges_names.size(), 0);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));
        int highest_time = 5;

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            // if (edge_alloc_in_first[edge_idx]) continue;
            // 选取排序存储slot
            std::vector<std::tuple<int, int, int>> sort_require(io.data_dm_rowstore.size());
            // 边缘节点能被分配到的客户端节点
            auto& clis = io.edge_dist_clients[edge_idx];
            // 排序每个边缘节点能够分配到的客户端的需求总值
            for (int j = 0; j < io.data_dm_rowstore.size(); j++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[j];
                int sumrow = 0;
                int max_stream_sum = 0;
                for (auto cli : clis) {
                    auto& cli_streams = row[cli];
                    int max_stream = 0;
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                        max_stream = std::max(cli_stream.first, max_stream);
                    }
                    max_stream_sum += max_stream;
                }
                sort_require[j] = {sumrow, max_stream_sum, j};
                // sort_require[j] = {max_stream_sum, sumrow, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx
            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                // int time = sort_require[require_idx].second;
                int time = std::get<2>(sort_require[require_idx]);
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
                // 计算当前edge节点在当前时间点分配了多少
                int dist_v_acc = 0;

                std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
                for(int idx=0; idx < clis.size(); idx++) {
                    int cli_idx = clis[idx];
                    auto& streams = row[cli_idx];
                    for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                        sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                    }
                }
                std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                for (auto& sort_stream: sort_cli_streams) {
                    int stream = std::get<0>(sort_stream);
                    if(stream == 0) break;
                    int stream_idx = std::get<1>(sort_stream);
                    int cli_idx = std::get<2>(sort_stream);
                    if (edge_rest_sb >= stream) {
                        res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                        edge_rest_sb -= stream;
                        row[cli_idx][stream_idx].first = 0;
                        dist_v_acc += stream;
                    }
                }
                if (dist_v_acc != 0)  {
                    five_count++;
                    edge_5_percent_time[edge_idx].push_back(time);
                }
                require_idx++;
            }
        }
        // 以上5%分配完

        calculate_each_edge_avg_dist_limit(avg_coefficient);
        // 开始第二轮分配，第二轮分配是面向客户节点分配
        // 对所有客户节点总需求量排序，以此选择时间点
        std::vector<std::tuple<int, int, int, int>> sort_stream;

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    sort_stream.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx, i});
                }
            }
        }
        // 从高到低排
        std::sort(sort_stream.begin(), sort_stream.end(), 
                    [](const std::tuple<int, int, int, int>& i,
                    const std::tuple<int, int, int, int>& j) { return i > j; });

        int pos_94 = 0.05 * res.size();
        std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
            io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

        for (int time = 0; time < res.size(); time++) {
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                sb_map_alltime[time][io.edges_names[edge_idx]];
                edge_sort_score[edge_idx][time] = {edge_dist, time};
            }
        }
        double sum = 0;
        for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
            auto edge_score = edge_sort_score[edge_idx];
            std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
            edge_94_dist[edge_idx] = edge_score[pos_94].first;
            
        }
        
        for (size_t st = 0; st < sort_stream.size(); st++) {
            int time = std::get<3>(sort_stream[st]);
            int cli_idx = std::get<2>(sort_stream[st]);
            int stream_idx = std::get<1>(sort_stream[st]);
            int stream = std::get<0>(sort_stream[st]);
            if(stream == 0) break;
            int edge_choose = calculate_best_edge_with_edge_limit(time, cli_idx, stream);
            if(edge_choose == -1) continue;
            
            int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
            auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

            // 添加结果
            res[time][cli_idx][edge_choose].push_back(cli_stream);
            // 分配值为客户端需求
            int dist_v = cli_stream.first;
            // 减去边缘节点剩余带宽
            edge_rest_sb -= dist_v;
            // 客户节点当前流的需求置0
            cli_stream.first = cli_stream.first - dist_v;

            int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
            if(edge_dist_now > edge_94_dist[edge_choose]) {
                bool is_in_5_percent = false;
                for (auto time_5 : edge_5_percent_time[edge_choose]) {
                    if(time == time_5) is_in_5_percent = true;
                }
                if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
            }
            edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            
        }

        sort_stream.clear();

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    sort_stream.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx, i});
                }
            }
        }
        // 从高到低排
        std::sort(sort_stream.begin(), sort_stream.end(), 
                    [](const std::tuple<int, int, int, int>& i,
                    const std::tuple<int, int, int, int>& j) { return i > j; });

        for (size_t st = 0; st < sort_stream.size(); st++) {
            int stream = std::get<0>(sort_stream[st]);
            if(stream == 0) break;
            int time = std::get<3>(sort_stream[st]);
            int cli_idx = std::get<2>(sort_stream[st]);
            int stream_idx = std::get<1>(sort_stream[st]);
            
            // // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
            // std::vector<std::tuple<int, int, int>> sort_array;
            // for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
            //     sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
            //                             io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
            //                             io.qos_map[cli_idx][i]});
            // }
            // // 注意这里是从高到低排，跟之前不一样
            // std::sort(sort_array.begin(), sort_array.end(),
            //             [](const std::tuple<int, int, int>& i,
            //                 const std::tuple<int, int, int>& j) { return i > j; });
            // int edge_choose = -1;
            // for(int i=0; i<sort_array.size(); i++) {
            //     int edge = std::get<2>(sort_array[i]);
            //     if(sb_map_alltime[time][io.edges_names[edge]] >= stream) {edge_choose = edge; break;}
            // }
            
            int edge_choose = calculate_best_edge(time, cli_idx, stream);
            // if(edge_choose == -1) continue;
            
            int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
            auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

            // 添加结果
            res[time][cli_idx][edge_choose].push_back(cli_stream);
            // 分配值为客户端需求
            int dist_v = cli_stream.first;
            // 减去边缘节点剩余带宽
            edge_rest_sb -= dist_v;
            // 客户节点当前流的需求置0
            cli_stream.first = cli_stream.first - dist_v;

            int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
            if(edge_dist_now > edge_94_dist[edge_choose]) {
                bool is_in_5_percent = false;
                for (auto time_5 : edge_5_percent_time[edge_choose]) {
                    if(time == time_5) is_in_5_percent = true;
                }
                if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
            }
            edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            
        }
        
        return;
    }

    // 全5%+纯暴力分，线上77w，佳玺2000w
    void brute_force6() {
        // this->calculate_each_edge_avg_upper_bound(avg_cost);
        edge_5_percent_time.resize(io.edges_names.size());
        edge_94_dist.resize(io.edges_names.size(), 0);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));
        int highest_time = 5;

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        // std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
        //     io.edges_names.size(),
        //     std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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
            // 如果第一次尝试二轮分配时分配过，则跳过
            // if (edge_alloc_in_first[edge_idx]) continue;
            // 选取排序存储slot
            std::vector<std::tuple<int, int, int>> sort_require(io.data_dm_rowstore.size());
            // 边缘节点能被分配到的客户端节点
            auto& clis = io.edge_dist_clients[edge_idx];
            // 排序每个边缘节点能够分配到的客户端的需求总值
            for (int j = 0; j < io.data_dm_rowstore.size(); j++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[j];
                int sumrow = 0;
                int max_stream_sum = 0;
                for (auto cli : clis) {
                    auto& cli_streams = row[cli];
                    int max_stream = 0;
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                        max_stream = std::max(cli_stream.first, max_stream);
                    }
                    max_stream_sum += max_stream;
                }
                // sort_require[j] = {sumrow, max_stream, j};
                // sort_require[j] = {max_stream_sum, sumrow, j};
                sort_require[j] = {sumrow, max_stream_sum, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx
            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                // int time = sort_require[require_idx].second;
                int time = std::get<2>(sort_require[require_idx]);
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
                // 计算当前edge节点在当前时间点分配了多少
                int dist_v_acc = 0;

                std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
                for(int idx=0; idx < clis.size(); idx++) {
                    int cli_idx = clis[idx];
                    auto& streams = row[cli_idx];
                    for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                        sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                    }
                }
                std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                for (auto& sort_stream: sort_cli_streams) {
                    int stream = std::get<0>(sort_stream);
                    if(stream == 0) break;
                    int stream_idx = std::get<1>(sort_stream);
                    int cli_idx = std::get<2>(sort_stream);
                    if (edge_rest_sb >= stream) {
                        res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                        edge_rest_sb -= stream;
                        row[cli_idx][stream_idx].first = 0;
                        dist_v_acc += stream;
                    }
                }
                if (dist_v_acc != 0)  {
                    five_count++;
                    edge_5_percent_time[edge_idx].push_back(time);
                }
                require_idx++;
            }
        }
        // 以上5%分配完

        // 开始第二轮分配，第二轮分配是面向客户节点分配
        // 对所有客户节点总需求量排序，以此选择时间点
        std::vector<std::tuple<int, int, int, int>> sort_stream;

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    sort_stream.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx, i});
                }
            }
        }
        // 从高到低排
        std::sort(sort_stream.begin(), sort_stream.end(), 
                    [](const std::tuple<int, int, int, int>& i,
                    const std::tuple<int, int, int, int>& j) { return i > j; });

        int pos_94 = 0.05 * res.size();
        std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
            io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

        for (int time = 0; time < res.size(); time++) {
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                sb_map_alltime[time][io.edges_names[edge_idx]];
                edge_sort_score[edge_idx][time] = {edge_dist, time};
            }
        }
        double sum = 0;
        for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
            auto edge_score = edge_sort_score[edge_idx];
            std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
            edge_94_dist[edge_idx] = edge_score[pos_94].first;
            
        }
        for (size_t st = 0; st < sort_stream.size(); st++) {
            int time = std::get<3>(sort_stream[st]);
            int cli_idx = std::get<2>(sort_stream[st]);
            int stream_idx = std::get<1>(sort_stream[st]);
            int stream = std::get<0>(sort_stream[st]);
            // if(st < 100) std::cout << st << " " << stream << std::endl;
            if(stream == 0) break;
            int edge_choose = calculate_best_edge3(time, cli_idx, stream);
            
            int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
            auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

            // 添加结果
            res[time][cli_idx][edge_choose].push_back(cli_stream);
            // 分配值为客户端需求
            int dist_v = cli_stream.first;
            // 减去边缘节点剩余带宽
            edge_rest_sb -= dist_v;
            // 客户节点当前流的需求置0
            cli_stream.first = cli_stream.first - dist_v;

            int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
            if(edge_dist_now > edge_94_dist[edge_choose]) {
                bool is_in_5_percent = false;
                for (auto time_5 : edge_5_percent_time[edge_choose]) {
                    if(time == time_5) is_in_5_percent = true;
                }
                if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
            }
            edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            
        }
        return;
    }

    // choose edge 线上调试first参数0.84, second参数0.5 + redist 489162
    void brute_force7(double edge_choose_rate_first, double edge_choose_rate_second) {
        // this->calculate_each_edge_avg_upper_bound(avg_cost);
        int edge_choose_num_first = io.edges_names.size() * edge_choose_rate_first;
        int edge_choose_num = io.edges_names.size() * edge_choose_rate_second;
        edge_choosed_and_sort_by_dist_num_first_round_bitmap.resize(io.edges_names.size(), false);
        edge_choosed_and_sort_by_dist_num_bitmap.resize(io.edges_names.size(), false);
        edge_choosed_and_sort_by_dist_num.resize(edge_choose_num);

        std::vector<std::pair<int, int>> edge_dist_sort(io.edges_names.size());
        for(int i=0; i<io.edges_names.size(); i++) {
            edge_dist_sort[i] = {io.edge_dist_num[i], i};
        }
        std::sort(edge_dist_sort.begin(), edge_dist_sort.end(), std::greater<std::pair<int, int>>());

        
        for(int i=0; i<edge_choose_num; i++) {
            edge_choosed_and_sort_by_dist_num_bitmap[edge_dist_sort[i].second] = true;
            edge_choosed_and_sort_by_dist_num[i] = edge_dist_sort[i].second;
        }

        edge_5_percent_time.resize(io.edges_names.size());
        edge_94_dist.resize(io.edges_names.size(), 0);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));
        int highest_time = 5;

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

        // 开始第一轮分配，第一轮分配是面向边缘节点分配
        // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
        
        std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
        for (size_t i = 0; i < io.edges_names.size(); i++) {
            sort_edge_avg_dist_highest_value.push_back(
                // {io.edge_dist_num[i], i});
                {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
                // {io.sb_map[io.edges_names[i]], i});
        }
        // 从高到低排
        std::sort(sort_edge_avg_dist_highest_value.begin(), sort_edge_avg_dist_highest_value.end(),
                  std::greater<std::pair<double, int>>());

        for (int i=0; i<edge_choose_num_first; i++) {
            int start = sort_edge_avg_dist_highest_value.size() - edge_choose_num_first;
            edge_choosed_and_sort_by_dist_num_first_round_bitmap[sort_edge_avg_dist_highest_value[start + i].second] = true;
        }
        // for (int i=0; i<edge_choose_num_first; i++) {
        //     edge_choosed_and_sort_by_dist_num_first_round_bitmap[sort_edge_avg_dist_highest_value[i].second] = true;
        // }

        for (int i = 0; i < sort_edge_avg_dist_highest_value.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            if(!edge_choosed_and_sort_by_dist_num_first_round_bitmap[edge_idx]) continue;
            // 如果第一次尝试二轮分配时分配过，则跳过
            // if (edge_alloc_in_first[edge_idx]) continue;
            // 选取排序存储slot
            std::vector<std::tuple<int, int, int>> sort_require(io.data_dm_rowstore.size());
            // 边缘节点能被分配到的客户端节点
            auto& clis = io.edge_dist_clients[edge_idx];
            // 排序每个边缘节点能够分配到的客户端的需求总值
            for (int j = 0; j < io.data_dm_rowstore.size(); j++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[j];
                int sumrow = 0;
                int max_stream_sum = 0;
                for (auto cli : clis) {
                    auto& cli_streams = row[cli];
                    int max_stream = 0;
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                        max_stream = std::max(cli_stream.first, max_stream);
                    }
                    max_stream_sum += max_stream;
                }
                // sort_require[j] = {sumrow, max_stream, j};
                // sort_require[j] = {max_stream_sum, sumrow, j};
                sort_require[j] = {sumrow, max_stream_sum, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx
            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                // int time = sort_require[require_idx].second;
                int time = std::get<2>(sort_require[require_idx]);
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
                // 计算当前edge节点在当前时间点分配了多少
                int dist_v_acc = 0;

                std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
                for(int idx=0; idx < clis.size(); idx++) {
                    int cli_idx = clis[idx];
                    auto& streams = row[cli_idx];
                    for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                        sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                    }
                }
                std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                for (auto& sort_stream: sort_cli_streams) {
                    int stream = std::get<0>(sort_stream);
                    if(stream == 0) break;
                    int stream_idx = std::get<1>(sort_stream);
                    int cli_idx = std::get<2>(sort_stream);
                    if (edge_rest_sb >= stream) {
                        res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                        edge_rest_sb -= stream;
                        row[cli_idx][stream_idx].first = 0;
                        dist_v_acc += stream;
                    }
                }
                if (dist_v_acc != 0)  {
                    five_count++;
                    edge_5_percent_time[edge_idx].push_back(time);
                }
                require_idx++;
            }
        }
        // 以上5%分配完

        // 开始第二轮分配，第二轮分配是面向客户节点分配
        // 对所有客户节点总需求量排序，以此选择时间点
        std::vector<std::tuple<int, int, int, int>> sort_stream;

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    sort_stream.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx, i});
                }
            }
        }
        // 从高到低排
        std::sort(sort_stream.begin(), sort_stream.end(), 
                    [](const std::tuple<int, int, int, int>& i,
                    const std::tuple<int, int, int, int>& j) { return i > j; });

        int pos_94 = 0.05 * res.size();
        std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
            io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

        for (int time = 0; time < res.size(); time++) {
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                sb_map_alltime[time][io.edges_names[edge_idx]];
                edge_sort_score[edge_idx][time] = {edge_dist, time};
            }
        }
        double sum = 0;
        for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
            auto edge_score = edge_sort_score[edge_idx];
            std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
            edge_94_dist[edge_idx] = edge_score[pos_94].first;
            
        }
        for (size_t st = 0; st < sort_stream.size(); st++) {
            int time = std::get<3>(sort_stream[st]);
            int cli_idx = std::get<2>(sort_stream[st]);
            int stream_idx = std::get<1>(sort_stream[st]);
            int stream = std::get<0>(sort_stream[st]);
            // if(st < 100) std::cout << st << " " << stream << std::endl;
            if(stream == 0) break;
            int edge_choose = calculate_best_edge_with_choose_edge(time, cli_idx, stream);
            if (edge_choose == -1) continue;
            int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
            auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

            // 添加结果
            res[time][cli_idx][edge_choose].push_back(cli_stream);
            // 分配值为客户端需求
            int dist_v = cli_stream.first;
            // 减去边缘节点剩余带宽
            edge_rest_sb -= dist_v;
            // 客户节点当前流的需求置0
            cli_stream.first = cli_stream.first - dist_v;

            int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
            if(edge_dist_now > edge_94_dist[edge_choose]) {
                bool is_in_5_percent = false;
                for (auto time_5 : edge_5_percent_time[edge_choose]) {
                    if(time == time_5) is_in_5_percent = true;
                }
                if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
            }
            edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            
        }

        sort_stream.clear();

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    sort_stream.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx, i});
                }
            }
        }

        // 从高到低排
        std::sort(sort_stream.begin(), sort_stream.end(), 
                    [](const std::tuple<int, int, int, int>& i,
                    const std::tuple<int, int, int, int>& j) { return i > j; });

        for (size_t st = 0; st < sort_stream.size(); st++) {
            int time = std::get<3>(sort_stream[st]);
            int cli_idx = std::get<2>(sort_stream[st]);
            int stream_idx = std::get<1>(sort_stream[st]);
            int stream = std::get<0>(sort_stream[st]);
            // if(st < 100) std::cout << st << " " << stream << std::endl;
            if(stream == 0) break;
            int edge_choose = calculate_best_edge3(time, cli_idx, stream);
            int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
            auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

            // 添加结果
            res[time][cli_idx][edge_choose].push_back(cli_stream);
            // 分配值为客户端需求
            int dist_v = cli_stream.first;
            // 减去边缘节点剩余带宽
            edge_rest_sb -= dist_v;
            // 客户节点当前流的需求置0
            cli_stream.first = cli_stream.first - dist_v;

            int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
            if(edge_dist_now > edge_94_dist[edge_choose]) {
                bool is_in_5_percent = false;
                for (auto time_5 : edge_5_percent_time[edge_choose]) {
                    if(time == time_5) is_in_5_percent = true;
                }
                if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
            }
            edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            
        }

        return;
    }


    // 
    void brute_force7_with_edge_limit(double avg_coefficient, double edge_choose_rate) {
        // this->calculate_each_edge_avg_upper_bound(avg_cost);
        int edge_choose_num = io.edges_names.size() * edge_choose_rate;
        edge_choosed_and_sort_by_dist_num_bitmap.resize(io.edges_names.size(), false);
        edge_choosed_and_sort_by_dist_num.resize(edge_choose_num);

        std::vector<std::pair<int, int>> edge_dist_sort(io.edges_names.size());
        for(int i=0; i<io.edges_names.size(); i++) {
            edge_dist_sort[i] = {io.edge_dist_num[i], i};
        }
        std::sort(edge_dist_sort.begin(), edge_dist_sort.end(), std::greater<std::pair<int, int>>());

        
        for(int i=0; i<edge_choose_num; i++) {
            edge_choosed_and_sort_by_dist_num_bitmap[edge_dist_sort[i].second] = true;
            edge_choosed_and_sort_by_dist_num[i] = edge_dist_sort[i].second;
        }

        edge_5_percent_time.resize(io.edges_names.size());
        edge_94_dist.resize(io.edges_names.size(), 0);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));
        int highest_time = 5;

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

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

        for (int i = 0; i < sort_edge_avg_dist_highest_value.size(); i++) {
            int edge_idx = edge_choosed_and_sort_by_dist_num[edge_choosed_and_sort_by_dist_num.size() - 1 - i];
            // 如果第一次尝试二轮分配时分配过，则跳过
            // if (edge_alloc_in_first[edge_idx]) continue;
            // 选取排序存储slot
            std::vector<std::tuple<int, int, int>> sort_require(io.data_dm_rowstore.size());
            // 边缘节点能被分配到的客户端节点
            auto& clis = io.edge_dist_clients[edge_idx];
            // 排序每个边缘节点能够分配到的客户端的需求总值
            for (int j = 0; j < io.data_dm_rowstore.size(); j++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[j];
                int sumrow = 0;
                int max_stream_sum = 0;
                for (auto cli : clis) {
                    auto& cli_streams = row[cli];
                    int max_stream = 0;
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                        max_stream = std::max(cli_stream.first, max_stream);
                    }
                    max_stream_sum += max_stream;
                }
                // sort_require[j] = {sumrow, max_stream, j};
                // sort_require[j] = {max_stream_sum, sumrow, j};
                sort_require[j] = {sumrow, max_stream_sum, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx
            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                // int time = sort_require[require_idx].second;
                int time = std::get<2>(sort_require[require_idx]);
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
                // 计算当前edge节点在当前时间点分配了多少
                int dist_v_acc = 0;

                std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
                for(int idx=0; idx < clis.size(); idx++) {
                    int cli_idx = clis[idx];
                    auto& streams = row[cli_idx];
                    for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                        sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                    }
                }
                std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                for (auto& sort_stream: sort_cli_streams) {
                    int stream = std::get<0>(sort_stream);
                    if(stream == 0) break;
                    int stream_idx = std::get<1>(sort_stream);
                    int cli_idx = std::get<2>(sort_stream);
                    if (edge_rest_sb >= stream) {
                        res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                        edge_rest_sb -= stream;
                        row[cli_idx][stream_idx].first = 0;
                        dist_v_acc += stream;
                    }
                }
                if (dist_v_acc != 0)  {
                    five_count++;
                    edge_5_percent_time[edge_idx].push_back(time);
                }
                require_idx++;
            }
        }
        // 以上5%分配完

        calculate_each_edge_avg_dist_limit(avg_coefficient);
        // 开始第二轮分配，第二轮分配是面向客户节点分配
        // 对所有客户节点总需求量排序，以此选择时间点
        std::vector<std::tuple<int, int, int, int>> sort_stream;

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    sort_stream.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx, i});
                }
            }
        }
        // 从高到低排
        std::sort(sort_stream.begin(), sort_stream.end(), 
                    [](const std::tuple<int, int, int, int>& i,
                    const std::tuple<int, int, int, int>& j) { return i > j; });

        int pos_94 = 0.05 * res.size();
        std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
            io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

        for (int time = 0; time < res.size(); time++) {
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                sb_map_alltime[time][io.edges_names[edge_idx]];
                edge_sort_score[edge_idx][time] = {edge_dist, time};
            }
        }
        double sum = 0;
        for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
            auto edge_score = edge_sort_score[edge_idx];
            std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
            edge_94_dist[edge_idx] = edge_score[pos_94].first;
            
        }
        for (size_t st = 0; st < sort_stream.size(); st++) {
            int time = std::get<3>(sort_stream[st]);
            int cli_idx = std::get<2>(sort_stream[st]);
            int stream_idx = std::get<1>(sort_stream[st]);
            int stream = std::get<0>(sort_stream[st]);
            // if(st < 100) std::cout << st << " " << stream << std::endl;
            if(stream == 0) break;
            int edge_choose = calculate_best_edge_with_edge_limit_with_choose_edge(time, cli_idx, stream);
            if (edge_choose == -1) continue;
            int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
            auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

            // 添加结果
            res[time][cli_idx][edge_choose].push_back(cli_stream);
            // 分配值为客户端需求
            int dist_v = cli_stream.first;
            // 减去边缘节点剩余带宽
            edge_rest_sb -= dist_v;
            // 客户节点当前流的需求置0
            cli_stream.first = cli_stream.first - dist_v;

            int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
            if(edge_dist_now > edge_94_dist[edge_choose]) {
                bool is_in_5_percent = false;
                for (auto time_5 : edge_5_percent_time[edge_choose]) {
                    if(time == time_5) is_in_5_percent = true;
                }
                if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
            }
            edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            
        }

        sort_stream.clear();

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    sort_stream.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx, i});
                }
            }
        }
        // 从高到低排
        std::sort(sort_stream.begin(), sort_stream.end(), 
                    [](const std::tuple<int, int, int, int>& i,
                    const std::tuple<int, int, int, int>& j) { return i > j; });

        for (size_t st = 0; st < sort_stream.size(); st++) {
            int stream = std::get<0>(sort_stream[st]);
            if(stream == 0) break;
            int time = std::get<3>(sort_stream[st]);
            int cli_idx = std::get<2>(sort_stream[st]);
            int stream_idx = std::get<1>(sort_stream[st]);
            
            // // 对客户端节点能分配到的边缘节点，根据边缘节点的连通性排序
            // std::vector<std::tuple<int, int, int>> sort_array;
            // for (size_t i = 0; i < io.qos_map[cli_idx].size(); i++) {
            //     sort_array.push_back({io.edge_dist_num[io.qos_map[cli_idx][i]],
            //                             io.sb_map[io.edges_names[io.qos_map[cli_idx][i]]],
            //                             io.qos_map[cli_idx][i]});
            // }
            // // 注意这里是从高到低排，跟之前不一样
            // std::sort(sort_array.begin(), sort_array.end(),
            //             [](const std::tuple<int, int, int>& i,
            //                 const std::tuple<int, int, int>& j) { return i > j; });
            // int edge_choose = -1;
            // for(int i=0; i<sort_array.size(); i++) {
            //     int edge = std::get<2>(sort_array[i]);
            //     if(sb_map_alltime[time][io.edges_names[edge]] >= stream) {edge_choose = edge; break;}
            // }
            
            int edge_choose = calculate_best_edge(time, cli_idx, stream);
            // if(edge_choose == -1) continue;
            
            int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
            auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

            // 添加结果
            res[time][cli_idx][edge_choose].push_back(cli_stream);
            // 分配值为客户端需求
            int dist_v = cli_stream.first;
            // 减去边缘节点剩余带宽
            edge_rest_sb -= dist_v;
            // 客户节点当前流的需求置0
            cli_stream.first = cli_stream.first - dist_v;

            int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
            if(edge_dist_now > edge_94_dist[edge_choose]) {
                bool is_in_5_percent = false;
                for (auto time_5 : edge_5_percent_time[edge_choose]) {
                    if(time == time_5) is_in_5_percent = true;
                }
                if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
            }
            edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            
        }
        return;
    }


    // 加快排序，但是线上会降到60w
    void brute_force8(double edge_choose_rate_first, double edge_choose_rate_second) {
        // this->calculate_each_edge_avg_upper_bound(avg_cost);
        int edge_choose_num_first = io.edges_names.size() * edge_choose_rate_first;
        int edge_choose_num = io.edges_names.size() * edge_choose_rate_second;
        edge_choosed_and_sort_by_dist_num_first_round_bitmap.resize(io.edges_names.size(), false);
        edge_choosed_and_sort_by_dist_num_bitmap.resize(io.edges_names.size(), false);
        edge_choosed_and_sort_by_dist_num.resize(edge_choose_num);

        std::vector<std::pair<int, int>> edge_dist_sort(io.edges_names.size());
        for(int i=0; i<io.edges_names.size(); i++) {
            edge_dist_sort[i] = {io.edge_dist_num[i], i};
        }
        std::sort(edge_dist_sort.begin(), edge_dist_sort.end(), std::greater<std::pair<int, int>>());

        
        for(int i=0; i<edge_choose_num; i++) {
            edge_choosed_and_sort_by_dist_num_bitmap[edge_dist_sort[i].second] = true;
            edge_choosed_and_sort_by_dist_num[i] = edge_dist_sort[i].second;
        }

        edge_5_percent_time.resize(io.edges_names.size());
        edge_94_dist.resize(io.edges_names.size(), 0);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));
        int highest_time = 5;

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

        // 开始第一轮分配，第一轮分配是面向边缘节点分配
        // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
        
        std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
        for (size_t i = 0; i < io.edges_names.size(); i++) {
            sort_edge_avg_dist_highest_value.push_back(
                // {io.edge_dist_num[i], i});
                {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
                // {io.sb_map[io.edges_names[i]], i});
        }
        // 从高到低排
        std::sort(sort_edge_avg_dist_highest_value.begin(), sort_edge_avg_dist_highest_value.end(),
                  std::greater<std::pair<double, int>>());

        for (int i=0; i<edge_choose_num_first; i++) {
            int start = sort_edge_avg_dist_highest_value.size() - edge_choose_num_first;
            edge_choosed_and_sort_by_dist_num_first_round_bitmap[sort_edge_avg_dist_highest_value[start + i].second] = true;
        }
        // for (int i=0; i<edge_choose_num_first; i++) {
        //     edge_choosed_and_sort_by_dist_num_first_round_bitmap[sort_edge_avg_dist_highest_value[i].second] = true;
        // }

        for (int i = 0; i < sort_edge_avg_dist_highest_value.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            if(!edge_choosed_and_sort_by_dist_num_first_round_bitmap[edge_idx]) continue;
            // 如果第一次尝试二轮分配时分配过，则跳过
            // if (edge_alloc_in_first[edge_idx]) continue;
            // 选取排序存储slot
            std::vector<std::tuple<int, int, int>> sort_require(io.data_dm_rowstore.size());
            // 边缘节点能被分配到的客户端节点
            auto& clis = io.edge_dist_clients[edge_idx];
            // 排序每个边缘节点能够分配到的客户端的需求总值
            for (int j = 0; j < io.data_dm_rowstore.size(); j++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[j];
                int sumrow = 0;
                int max_stream_sum = 0;
                for (auto cli : clis) {
                    auto& cli_streams = row[cli];
                    int max_stream = 0;
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                        max_stream = std::max(cli_stream.first, max_stream);
                    }
                    max_stream_sum += max_stream;
                }
                // sort_require[j] = {sumrow, max_stream, j};
                // sort_require[j] = {max_stream_sum, sumrow, j};
                sort_require[j] = {sumrow, max_stream_sum, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx
            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                // int time = sort_require[require_idx].second;
                int time = std::get<2>(sort_require[require_idx]);
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
                // 计算当前edge节点在当前时间点分配了多少
                int dist_v_acc = 0;

                std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
                for(int idx=0; idx < clis.size(); idx++) {
                    int cli_idx = clis[idx];
                    auto& streams = row[cli_idx];
                    for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                        sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                    }
                }
                std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                for (auto& sort_stream: sort_cli_streams) {
                    int stream = std::get<0>(sort_stream);
                    if(stream == 0) break;
                    int stream_idx = std::get<1>(sort_stream);
                    int cli_idx = std::get<2>(sort_stream);
                    if (edge_rest_sb >= stream) {
                        res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                        edge_rest_sb -= stream;
                        row[cli_idx][stream_idx].first = 0;
                        dist_v_acc += stream;
                    }
                }
                if (dist_v_acc != 0)  {
                    five_count++;
                    edge_5_percent_time[edge_idx].push_back(time);
                }
                require_idx++;
            }
        }
        // 以上5%分配完
        // io.output_demand();
        // output_edge_dist();
        // 开始第二轮分配，第二轮分配是面向客户节点分配
        // 对所有客户节点总需求量排序，以此选择时间点

        std::vector<std::pair<int, int>> sort_time(io.data_dm_rowstore.size());
        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            int time_sum = 0;
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    time_sum += stream_idx;
                }
            }
            sort_time[i] = {time_sum, i};
        }
        std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

        int pos_94 = 0.05 * res.size();
        std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
            io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

        for (int time = 0; time < res.size(); time++) {
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                sb_map_alltime[time][io.edges_names[edge_idx]];
                edge_sort_score[edge_idx][time] = {edge_dist, time};
            }
        }
        double sum = 0;
        for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
            auto edge_score = edge_sort_score[edge_idx];
            std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
            edge_94_dist[edge_idx] = edge_score[pos_94].first;
            
        }
        for (size_t st = 0; st < sort_time.size(); st++) {
            if(sort_time[st].first == 0) break;
            int time = sort_time[st].second;
            auto& row = io.data_dm_rowstore[time];
            std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
            for(int cli_idx=0; cli_idx < io.client_names.size(); cli_idx++) {
                auto& streams = row[cli_idx];
                for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                    sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                }
            }
            std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                            const std::tuple<int, int, int>& j) { return i > j; });
            
            for(auto& sort_stream : sort_cli_streams) {
                int cli_idx = std::get<2>(sort_stream);
                int stream_idx = std::get<1>(sort_stream);
                int stream = std::get<0>(sort_stream);
                if(stream == 0) break;
                int edge_choose = calculate_best_edge_with_choose_edge(time, cli_idx, stream);
                if (edge_choose == -1) continue;
                int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
                auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

                // 添加结果
                res[time][cli_idx][edge_choose].push_back(cli_stream);
                // 分配值为客户端需求
                int dist_v = cli_stream.first;
                // 减去边缘节点剩余带宽
                edge_rest_sb -= dist_v;
                // 客户节点当前流的需求置0
                cli_stream.first = cli_stream.first - dist_v;

                int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
                if(edge_dist_now > edge_94_dist[edge_choose]) {
                    bool is_in_5_percent = false;
                    for (auto time_5 : edge_5_percent_time[edge_choose]) {
                        if(time == time_5) is_in_5_percent = true;
                    }
                    if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
                }
                edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            }
            // if(st == 0) output_edge_dist();
        }

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            int time_sum = 0;
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    time_sum += stream_idx;
                }
            }
            sort_time[i] = {time_sum, i};
        }
        std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

        for (size_t st = 0; st < sort_time.size(); st++) {
            if(sort_time[st].first == 0) break;
            int time = sort_time[st].second;
            auto& row = io.data_dm_rowstore[time];

            std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
            for(int cli_idx=0; cli_idx < io.client_names.size(); cli_idx++) {
                auto& streams = row[cli_idx];
                for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                    sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                }
            }
            std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                            const std::tuple<int, int, int>& j) { return i > j; });

            for(auto& sort_stream : sort_cli_streams) {
                int cli_idx = std::get<2>(sort_stream);
                int stream_idx = std::get<1>(sort_stream);
                int stream = std::get<0>(sort_stream);
                if(stream == 0) break;
                int edge_choose = calculate_best_edge3(time, cli_idx, stream);
                int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
                auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

                // 添加结果
                res[time][cli_idx][edge_choose].push_back(cli_stream);
                // 分配值为客户端需求
                int dist_v = cli_stream.first;
                // 减去边缘节点剩余带宽
                edge_rest_sb -= dist_v;
                // 客户节点当前流的需求置0
                cli_stream.first = cli_stream.first - dist_v;

                int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
                if(edge_dist_now > edge_94_dist[edge_choose]) {
                    bool is_in_5_percent = false;
                    for (auto time_5 : edge_5_percent_time[edge_choose]) {
                        if(time == time_5) is_in_5_percent = true;
                    }
                    if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
                }
                edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            }
        }
        // output_edge_dist();
        return;
    }


    // choose edge 线上调试first参数0.84, second参数0.5 + redist 485552
    // 先分配base_cost再分配5%
    int brute_force9(double edge_choose_rate_first, double edge_choose_rate_second) {
        // this->calculate_each_edge_avg_upper_bound(avg_cost);
        for(int i=0; i<io.qos_map[1].size(); i++) std::cout << "EDGE_IDX:" << io.qos_map[1][i] << "DIST_NUM:" << io.edge_dist_num[io.qos_map[1][i]] <<" ";
        std::cout << std::endl;

        for(int i=0; i<io.qos_map[5].size(); i++) std::cout << "EDGE_IDX:" << io.qos_map[5][i] << "DIST_NUM:" << io.edge_dist_num[io.qos_map[5][i]] <<" ";
        std::cout << std::endl;
        int edge_choose_num_first = io.edges_names.size() * edge_choose_rate_first;
        int edge_choose_num = io.edges_names.size() * edge_choose_rate_second;
        edge_choosed_and_sort_by_dist_num_first_round_bitmap.resize(io.edges_names.size(), false);
        edge_choosed_and_sort_by_dist_num_bitmap.resize(io.edges_names.size(), false);
        edge_choosed_and_sort_by_dist_num.resize(edge_choose_num);

        int total_require = 0;
        int edge_offer = 0;
        int max_cli_require_sum = 0;
        for (int cli_idx=0; cli_idx < io.client_names.size(); cli_idx++) {
            int max_cli_dm = 0;
            for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
                int cli_dm = 0;
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    cli_dm += row[cli_idx][stream_idx].first;
                }
                max_cli_dm = std::max(max_cli_dm, cli_dm);
            }
            max_cli_require_sum += max_cli_dm;
        }

        for(int i=0; i<io.edges_names.size(); i++) {
            if(io.edge_dist_num[i] != 0) edge_offer += io.sb_map[io.edges_names[i]];
        }

        // five_times_edge_offer *= 5;
        // std::cout << "Max Cli Require: " << max_cli_require_sum << " Edge Offer: " << edge_offer << " Ratio: " << double(edge_offer) / max_cli_require_sum << std::endl;

        std::vector<std::pair<int, int>> edge_dist_sort(io.edges_names.size());
        for(int i=0; i<io.edges_names.size(); i++) {
            edge_dist_sort[i] = {io.edge_dist_num[i], i};
        }
        std::sort(edge_dist_sort.begin(), edge_dist_sort.end(), std::greater<std::pair<int, int>>());

        
        for(int i=0; i<edge_choose_num; i++) {
            edge_choosed_and_sort_by_dist_num_bitmap[edge_dist_sort[i].second] = true;
            edge_choosed_and_sort_by_dist_num[i] = edge_dist_sort[i].second;
        }

        edge_5_percent_time.resize(io.edges_names.size());
        edge_94_dist.resize(io.edges_names.size(), 0);
        res.resize(
            io.data_dm_rowstore.size(),
            std::vector<std::vector<std::vector<std::pair<int, std::string>>>>(
                io.client_names.size(),
                std::vector<std::vector<std::pair<int, std::string>>>(io.edges_names.size())));
        int highest_time = 5;

        // !开始正式分配
        // 边缘节点分配的时间
        std::vector<int> edge_times(io.edges_names.size(), 0);

        // 百分之五的时间点个数
        int five_percent_time = io.data_dm_rowstore.size() * 0.05 - 1;

        int second_time = io.data_dm_rowstore.size() * 0.02 - 1;

        // 已经超分配到边
        std::vector<bool> edge_exceed(io.edges_names.size(), false);

        // edge节点超分配的最大值
        std::vector<int> edge_max_record(io.edges_names.size(), 0);

        // 对所有边缘节点可分配的客户端总需求排序存储的数据结构
        // edge_id / 时间点idx / {客户端需求的和，对应时间点idx}
        std::vector<std::vector<std::pair<int, int>>> edge_require_sort(
            io.edges_names.size(),
            std::vector<std::pair<int, int>>(io.data_dm_rowstore.size(), {0, 0}));

        // 开始第一轮分配，第一轮分配是面向边缘节点分配
        // 按照节点可分配的客户端能分配到的平均值，对边缘节点进行排序。加0.1是为了避免除0
        
        std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
        for (size_t i = 0; i < io.edges_names.size(); i++) {
            sort_edge_avg_dist_highest_value.push_back(
                // {io.edge_dist_num[i], i});
                {io.sb_map[io.edges_names[i]] / (io.edge_dist_num[i] + 0.1), i});
                // {io.sb_map[io.edges_names[i]], i});
        }
        // 从高到低排
        std::sort(sort_edge_avg_dist_highest_value.begin(), sort_edge_avg_dist_highest_value.end(),
                  std::greater<std::pair<double, int>>());

        for (int i=0; i<edge_choose_num_first; i++) {
            int start = sort_edge_avg_dist_highest_value.size() - edge_choose_num_first;
            edge_choosed_and_sort_by_dist_num_first_round_bitmap[sort_edge_avg_dist_highest_value[start + i].second] = true;
        }
        // for (int i=0; i<edge_choose_num_first; i++) {
        //     edge_choosed_and_sort_by_dist_num_first_round_bitmap[sort_edge_avg_dist_highest_value[i].second] = true;
        // }
        // 开始base_cost分配
        for (int i = 0; i < sort_edge_avg_dist_highest_value.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            if(!edge_choosed_and_sort_by_dist_num_first_round_bitmap[edge_idx]) continue;
            // // 如果第一次尝试二轮分配时分配过，则跳过
            // // if (edge_alloc_in_first[edge_idx]) continue;
            // // 选取排序存储slot
            // std::vector<std::tuple<int, int, int>> sort_require(io.data_dm_rowstore.size());
            // // 边缘节点能被分配到的客户端节点
            auto& clis = io.edge_dist_clients[edge_idx];
            // // 排序每个边缘节点能够分配到的客户端的需求总值
            // for (int j = 0; j < io.data_dm_rowstore.size(); j++) {
            //     std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[j];
            //     int sumrow = 0;
            //     int max_stream_sum = 0;
            //     for (auto cli : clis) {
            //         auto& cli_streams = row[cli];
            //         int max_stream = 0;
            //         for (auto& cli_stream : cli_streams) {
            //             sumrow += cli_stream.first;
            //             max_stream = std::max(cli_stream.first, max_stream);
            //         }
            //         max_stream_sum += max_stream;
            //     }
            //     // sort_require[j] = {sumrow, max_stream, j};
            //     // sort_require[j] = {max_stream_sum, sumrow, j};
            //     sort_require[j] = {sumrow, max_stream_sum, j};
            // }
            // // 也是从高到低排
            // std::sort(sort_require.begin(), sort_require.end(),
            //           [](const std::tuple<int, int, int>& i,
            //                  const std::tuple<int, int, int>& j) { return i > j; });

            // int require_idx = 0;  // 需求排序后遍历的idx
            for (int time=0; time < io.data_dm_rowstore.size(); time++) {
                // int time = sort_require[require_idx].second;
                // int time = std::get<2>(sort_require[require_idx]);
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
                // 计算当前edge节点在当前时间点分配了多少
                int dist_v_acc = 0;

                std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
                for(int idx=0; idx < clis.size(); idx++) {
                    int cli_idx = clis[idx];
                    auto& streams = row[cli_idx];
                    for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                        sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                    }
                }
                std::sort(sort_cli_streams.begin(), sort_cli_streams.end());

                int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                for (auto& sort_stream: sort_cli_streams) {
                    int stream = std::get<0>(sort_stream);
                    if(stream == 0) continue;
                    int stream_idx = std::get<1>(sort_stream);
                    int cli_idx = std::get<2>(sort_stream);
                    int diff_to_base_cost = io.base_cost - (io.sb_map[io.edges_names[edge_idx]] - edge_rest_sb);
                    if (diff_to_base_cost >= stream && edge_rest_sb >= stream) {
                        res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                        edge_rest_sb -= stream;
                        row[cli_idx][stream_idx].first = 0;
                        dist_v_acc += stream;
                    } else break;
                }
                // require_idx++;
            }
        }

        // 开始5%分配
        for (int i = 0; i < sort_edge_avg_dist_highest_value.size(); i++) {
            int edge_idx = sort_edge_avg_dist_highest_value[i].second;
            if(!edge_choosed_and_sort_by_dist_num_first_round_bitmap[edge_idx]) continue;
            // 如果第一次尝试二轮分配时分配过，则跳过
            // if (edge_alloc_in_first[edge_idx]) continue;
            // 选取排序存储slot
            std::vector<std::tuple<int, int, int>> sort_require(io.data_dm_rowstore.size());
            // 边缘节点能被分配到的客户端节点
            auto& clis = io.edge_dist_clients[edge_idx];
            // 排序每个边缘节点能够分配到的客户端的需求总值
            for (int j = 0; j < io.data_dm_rowstore.size(); j++) {
                std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[j];
                int sumrow = 0;
                int max_stream_sum = 0;
                for (auto cli : clis) {
                    auto& cli_streams = row[cli];
                    int max_stream = 0;
                    for (auto& cli_stream : cli_streams) {
                        sumrow += cli_stream.first;
                        max_stream = std::max(cli_stream.first, max_stream);
                    }
                    max_stream_sum += max_stream;
                }
                // sort_require[j] = {sumrow, max_stream, j};
                // sort_require[j] = {max_stream_sum, sumrow, j};
                sort_require[j] = {sumrow, max_stream_sum, j};
            }
            // 也是从高到低排
            std::sort(sort_require.begin(), sort_require.end(),
                      [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

            int five_count = 0;   // 边缘节点分配次数的计数
            int require_idx = 0;  // 需求排序后遍历的idx
            while (five_count <= five_percent_time && require_idx < sort_require.size()) {
                // int time = sort_require[require_idx].second;
                int time = std::get<2>(sort_require[require_idx]);
                // 获取当前时间点边缘节点剩余的带宽
                auto& sb_map_ref = sb_map_alltime[time];
                std::vector<std::vector<std::pair<int, std::string>>>& row =
                    io.data_dm_rowstore[time];
                // 计算当前edge节点在当前时间点分配了多少
                int dist_v_acc = 0;

                std::vector<std::tuple<int, int, int>> sort_cli_streams; // stream / stream_idx / cli_idx
                for(int idx=0; idx < clis.size(); idx++) {
                    int cli_idx = clis[idx];
                    auto& streams = row[cli_idx];
                    for(int stream_idx=0; stream_idx < streams.size(); stream_idx++) {
                        sort_cli_streams.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx});
                    }
                }
                std::sort(sort_cli_streams.begin(), sort_cli_streams.end(), [](const std::tuple<int, int, int>& i,
                             const std::tuple<int, int, int>& j) { return i > j; });

                int& edge_rest_sb = sb_map_ref[io.edges_names[edge_idx]];
                for (auto& sort_stream: sort_cli_streams) {
                    int stream = std::get<0>(sort_stream);
                    if(stream == 0) break;
                    int stream_idx = std::get<1>(sort_stream);
                    int cli_idx = std::get<2>(sort_stream);
                    if (edge_rest_sb >= stream) {
                        res[time][cli_idx][edge_idx].push_back(row[cli_idx][stream_idx]);
                        edge_rest_sb -= stream;
                        row[cli_idx][stream_idx].first = 0;
                        dist_v_acc += stream;
                    }
                }
                if (dist_v_acc != 0)  {
                    five_count++;
                    edge_5_percent_time[edge_idx].push_back(time);
                }
                require_idx++;
            }
        }
        // 以上5%分配完
        io.output_demand();

        // 开始第二轮分配，第二轮分配是面向客户节点分配
        // 对所有客户节点总需求量排序，以此选择时间点
        std::vector<std::tuple<int, int, int, int>> sort_stream;

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    sort_stream.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx, i});
                }
            }
        }
        // 从高到低排
        std::sort(sort_stream.begin(), sort_stream.end(), 
                    [](const std::tuple<int, int, int, int>& i,
                    const std::tuple<int, int, int, int>& j) { return i > j; });

        int pos_94 = 0.05 * res.size();
        std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
            io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

        for (int time = 0; time < res.size(); time++) {
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                sb_map_alltime[time][io.edges_names[edge_idx]];
                edge_sort_score[edge_idx][time] = {edge_dist, time};
            }
        }
        double sum = 0;
        for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
            auto edge_score = edge_sort_score[edge_idx];
            std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
            edge_94_dist[edge_idx] = edge_score[pos_94].first;
            
        }
        for (size_t st = 0; st < sort_stream.size(); st++) {
            int time = std::get<3>(sort_stream[st]);
            int cli_idx = std::get<2>(sort_stream[st]);
            int stream_idx = std::get<1>(sort_stream[st]);
            int stream = std::get<0>(sort_stream[st]);
            // if(st < 100) std::cout << st << " " << stream << std::endl;
            if(stream == 0) break;
            int edge_choose = calculate_best_edge_with_choose_edge(time, cli_idx, stream);
            if (edge_choose == -1) continue;
            int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
            auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

            // 添加结果
            res[time][cli_idx][edge_choose].push_back(cli_stream);
            // 分配值为客户端需求
            int dist_v = cli_stream.first;
            // 减去边缘节点剩余带宽
            edge_rest_sb -= dist_v;
            // 客户节点当前流的需求置0
            cli_stream.first = cli_stream.first - dist_v;

            int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
            if(edge_dist_now > edge_94_dist[edge_choose]) {
                bool is_in_5_percent = false;
                for (auto time_5 : edge_5_percent_time[edge_choose]) {
                    if(time == time_5) is_in_5_percent = true;
                }
                if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
            }
            edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            
        }

        sort_stream.clear();

        for (int i = 0; i < io.data_dm_rowstore.size(); i++) {
            std::vector<std::vector<std::pair<int, std::string>>>& row = io.data_dm_rowstore[i];
            for (int cli_idx=0; cli_idx < row.size(); cli_idx++) {
                for (int stream_idx=0; stream_idx < row[cli_idx].size(); stream_idx++) {
                    sort_stream.push_back({row[cli_idx][stream_idx].first, stream_idx, cli_idx, i});
                }
            }
        }

        // 从高到低排
        std::sort(sort_stream.begin(), sort_stream.end(), 
                    [](const std::tuple<int, int, int, int>& i,
                    const std::tuple<int, int, int, int>& j) { return i > j; });

        for (size_t st = 0; st < sort_stream.size(); st++) {
            int time = std::get<3>(sort_stream[st]);
            int cli_idx = std::get<2>(sort_stream[st]);
            int stream_idx = std::get<1>(sort_stream[st]);
            int stream = std::get<0>(sort_stream[st]);
            // if(st < 100) std::cout << st << " " << stream << std::endl;
            if(stream == 0) break;
            int edge_choose = calculate_best_edge3(time, cli_idx, stream);
            if (edge_choose == -1) return -1;
            int& edge_rest_sb = sb_map_alltime[time][io.edges_names[edge_choose]];
            auto& cli_stream = io.data_dm_rowstore[time][cli_idx][stream_idx];

            // 添加结果
            res[time][cli_idx][edge_choose].push_back(cli_stream);
            // 分配值为客户端需求
            int dist_v = cli_stream.first;
            // 减去边缘节点剩余带宽
            edge_rest_sb -= dist_v;
            // 客户节点当前流的需求置0
            cli_stream.first = cli_stream.first - dist_v;

            int edge_dist_now = io.sb_map[io.edges_names[edge_choose]] - edge_rest_sb;
            if(edge_dist_now > edge_94_dist[edge_choose]) {
                bool is_in_5_percent = false;
                for (auto time_5 : edge_5_percent_time[edge_choose]) {
                    if(time == time_5) is_in_5_percent = true;
                }
                if(!is_in_5_percent) edge_94_dist[edge_choose] = edge_dist_now;
            }
            edge_94_dist[edge_choose] = std::max(edge_94_dist[edge_choose], edge_dist_now);
            
        }

        output_edge_dist();
        return 0;
    }

    int calculate_best_edge(int time, int cli_idx, int stream) {
        auto& sb_map_ref = sb_map_alltime[time];
        auto edges_relate = io.qos_map[cli_idx];
        // std::vector<std::pair<int, int>> edge_dist_sort(edges_relate.size());
        // for(int i=0; i<edges_relate.size(); i++) {
        //     edge_dist_sort[i] = {io.edge_dist_num[edges_relate[i]], edges_relate[i]};
        // }
        // std::sort(edge_dist_sort.begin(), edge_dist_sort.end(), std::greater<std::pair<int, int>>());
        std::vector<std::tuple<int, int, int>> score_diff_sort; // score_diff / edge_dist_num / edge_id 
        for(int i=0; i < edges_relate.size(); i++) {
            // int edge_idx = edge_dist_sort[i].second;
            int edge_idx = edges_relate[i];
            int score_diff = 0;
            int dist_max_before = edge_94_dist[edge_idx];
            int dist_later =  io.sb_map[io.edges_names[edge_idx]] - sb_map_ref[io.edges_names[edge_idx]] + stream;
            if (dist_later > io.sb_map[io.edges_names[edge_idx]]) continue;
            int score_before = 0;
            int score_later = 0;
            
            if (dist_max_before == 0) {
                score_before += 0;
            } else if (dist_max_before <= io.base_cost) {
                score_before += io.base_cost;
            } else {
                int64_t over = dist_max_before - io.base_cost;
                score_before += dist_max_before;
                score_before += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
            }

            bool is_in_5_percent = false;
            for (auto time_5 : edge_5_percent_time[edge_idx]) {
                if(time == time_5) is_in_5_percent = true;
            }
            if(is_in_5_percent) score_later = -1;
            else if (dist_later == 0) {
                score_later = 0;
            } else if (dist_later <= io.base_cost) {
                score_later = io.base_cost;
            } else {
                int64_t over = dist_later - io.base_cost;
                score_later += dist_later;
                score_later += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
            }
            score_diff = score_later <= score_before ? 0 : score_later - score_before;
            score_diff_sort.push_back({score_diff, io.edge_dist_num[edge_idx], edge_idx});
        }
        std::sort(score_diff_sort.begin(), score_diff_sort.end(),
                          [](const std::tuple<int, int, int>& i,                    
                             const std::tuple<int, int, int>& j) { 
                                 if(std::get<0>(i) == std::get<0>(j)) {
                                     if(std::get<0>(i) >= 0) return std::get<1>(i) > std::get<1>(j);
                                     else return std::get<1>(i) < std::get<1>(j);
                                 }
                                 else 
                                    return i < j; });
        return std::get<2>(score_diff_sort[0]);
    }

    int calculate_best_edge_with_edge_limit(int time, int cli_idx, int stream) {
        auto& sb_map_ref = sb_map_alltime[time];
        auto edges_relate = io.qos_map[cli_idx];
        std::vector<std::tuple<int, int, int>> score_diff_sort; // score_diff / edge_dist_num / edge_id 
        
        for(int i=0; i < edges_relate.size(); i++) {
            int edge_idx = edges_relate[i];
            int score_diff = 0;
            int dist_max_before = edge_94_dist[edge_idx];
            int dist_later =  io.sb_map[io.edges_names[edge_idx]] - sb_map_ref[io.edges_names[edge_idx]] + stream;
            if (dist_later > io.sb_map[io.edges_names[edge_idx]] || dist_later > edges_avg_limit[edge_idx]) continue;
            int score_before = 0;
            int score_later = 0;

            
            if (dist_max_before == 0) {
                score_before += 0;
            } else if (dist_max_before <= io.base_cost) {
                score_before += io.base_cost;
            } else {
                int64_t over = dist_max_before - io.base_cost;
                score_before += dist_max_before;
                score_before += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
            }

            bool is_in_5_percent = false;
            for (auto time_5 : edge_5_percent_time[edge_idx]) {
                if(time == time_5) is_in_5_percent = true;
            }
            if(is_in_5_percent) score_later = -1;
            else if (dist_later == 0) {
                score_later = 0;
            } else if (dist_later <= io.base_cost) {
                score_later = io.base_cost;
            } else {
                int64_t over = dist_later - io.base_cost;
                score_later += dist_later;
                score_later += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
            }
            score_diff = score_later <= score_before ? 0 : score_later - score_before;
            score_diff_sort.push_back({score_diff, io.edge_dist_num[edge_idx], edge_idx});
        }
        std::sort(score_diff_sort.begin(), score_diff_sort.end(),
                          [](const std::tuple<int, int, int>& i,                    
                             const std::tuple<int, int, int>& j) { 
                                 if(std::get<0>(i) == std::get<0>(j)) {
                                     if(std::get<0>(i) >= 0) return std::get<1>(i) > std::get<1>(j);
                                     else return std::get<1>(i) < std::get<1>(j);
                                 }
                                 else return i < j; });
        if (score_diff_sort.size() == 0) return -1;
        else return std::get<2>(score_diff_sort[0]);
    }

    int calculate_best_edge3(int time, int cli_idx, int stream) {
        auto& sb_map_ref = sb_map_alltime[time];
        auto edges_relate = io.qos_map[cli_idx];
        std::vector<std::pair<int, int>> edge_dist_sort(edges_relate.size());
        for(int i=0; i<edges_relate.size(); i++) {
            edge_dist_sort[i] = {io.edge_dist_num[edges_relate[i]], edges_relate[i]};
        }
        std::sort(edge_dist_sort.begin(), edge_dist_sort.end(), std::greater<std::pair<int, int>>());
        // for(int i=0; i<edge_dist_sort.size(); i++) {
        //     int edge_idx = edge_dist_sort[i].second;
        //     if(!edge_choosed_and_sort_by_dist_num_first_round_bitmap[edge_idx]) continue;
        //     if(io.base_cost - (io.sb_map[io.edges_names[edge_idx]] - sb_map_ref[io.edges_names[edge_idx]]) >= stream) return edge_idx;
        // }
        for(int i=0; i<edge_dist_sort.size(); i++) {
            int edge_idx = edge_dist_sort[i].second;
            if(!edge_choosed_and_sort_by_dist_num_first_round_bitmap[edge_idx]) continue;
            if(sb_map_ref[io.edges_names[edge_idx]] >= stream) return edge_idx;
        }
        return -1;
        // return edge_dist_sort[0].second;
        // std::vector<std::tuple<int, int, int>> score_diff_sort; // score_diff / edge_dist_num / edge_id 
        // for(int i=0; i < edges_relate.size(); i++) {
        //     // int edge_idx = edge_dist_sort[i].second;
        //     int edge_idx = edges_relate[i];
        //     int score_diff = 0;
        //     int dist_max_before = edge_94_dist[edge_idx];
        //     int dist_later =  io.sb_map[io.edges_names[edge_idx]] - sb_map_ref[io.edges_names[edge_idx]] + stream;
        //     if (dist_later > io.sb_map[io.edges_names[edge_idx]]) continue;
        //     int score_before = 0;
        //     int score_later = 0;
            
        //     if (dist_max_before == 0) {
        //         score_before += 0;
        //     } else if (dist_max_before <= io.base_cost) {
        //         score_before += io.base_cost;
        //     } else {
        //         int64_t over = dist_max_before - io.base_cost;
        //         score_before += dist_max_before;
        //         score_before += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
        //     }

        //     bool is_in_5_percent = false;
        //     for (auto time_5 : edge_5_percent_time[edge_idx]) {
        //         if(time == time_5) is_in_5_percent = true;
        //     }
        //     if(is_in_5_percent) score_later = -1;
        //     else if (dist_later == 0) {
        //         score_later = 0;
        //     } else if (dist_later <= io.base_cost) {
        //         score_later = io.base_cost;
        //     } else {
        //         int64_t over = dist_later - io.base_cost;
        //         score_later += dist_later;
        //         score_later += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
        //     }
        //     score_diff = score_later <= score_before ? 0 : score_later - score_before;
        //     score_diff_sort.push_back({score_diff, io.edge_dist_num[edge_idx], edge_idx});
        // }
        // std::sort(score_diff_sort.begin(), score_diff_sort.end(),
        //                   [](const std::tuple<int, int, int>& i,                    
        //                      const std::tuple<int, int, int>& j) { 
        //                          if(std::get<0>(i) == std::get<0>(j)) {
        //                              if(std::get<0>(i) >= 0) return std::get<1>(i) > std::get<1>(j);
        //                              else return std::get<1>(i) < std::get<1>(j);
        //                          }
        //                          else return i < j; });
        // return std::get<2>(score_diff_sort[0]);
    }


    int calculate_best_edge_with_choose_edge(int time, int cli_idx, int stream) {
        auto& sb_map_ref = sb_map_alltime[time];
        auto edges_relate = io.qos_map[cli_idx];
        // std::vector<std::pair<int, int>> edge_dist_sort(edges_relate.size());
        // for(int i=0; i<edges_relate.size(); i++) {
        //     edge_dist_sort[i] = {io.edge_dist_num[edges_relate[i]], edges_relate[i]};
        // }
        // std::sort(edge_dist_sort.begin(), edge_dist_sort.end(), std::greater<std::pair<int, int>>());
        
        // for(int i=0; i<edges_relate.size(); i++) {
        //     int edge_idx = edges_relate[i];
        //     if(!edge_choosed_and_sort_by_dist_num_first_round_bitmap[edge_idx]) continue;
        //     if(io.base_cost - (io.sb_map[io.edges_names[edge_idx]] - sb_map_ref[io.edges_names[edge_idx]]) >= stream) return edge_idx;
        // }
        std::vector<std::tuple<int, int, int>> score_diff_sort; // score_diff / edge_dist_num / edge_id 
        int min_score_diff = INT32_MAX;
        int max_dist_num = 0;
        int choose_now = -1;
        for(int i=0; i < edges_relate.size(); i++) {
            // int edge_idx = edge_dist_sort[i].second;
            int edge_idx = edges_relate[i];
            if(!edge_choosed_and_sort_by_dist_num_bitmap[edge_idx] || !edge_choosed_and_sort_by_dist_num_first_round_bitmap[edge_idx]) continue;
            int score_diff = 0;
            int dist_max_before = edge_94_dist[edge_idx];
            int dist_later =  io.sb_map[io.edges_names[edge_idx]] - sb_map_ref[io.edges_names[edge_idx]] + stream;
            if (dist_later > io.sb_map[io.edges_names[edge_idx]]) continue;
            int score_before = 0;
            int score_later = 0;
            
            if (dist_max_before == 0) {
                score_before += 0;
            } else if (dist_max_before <= io.base_cost) {
                score_before += io.base_cost;
            } else {
                int64_t over = dist_max_before - io.base_cost;
                score_before += dist_max_before;
                score_before += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
            }

            bool is_in_5_percent = false;
            for (auto time_5 : edge_5_percent_time[edge_idx]) {
                if(time == time_5) is_in_5_percent = true;
            }
            if(is_in_5_percent) score_later = -1;
            else if (dist_later == 0) {
                score_later = 0;
            } else if (dist_later <= io.base_cost) {
                score_later = io.base_cost;
            } else {
                int64_t over = dist_later - io.base_cost;
                score_later += dist_later;
                score_later += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
            }
            if(score_later == -1) score_diff = -1;
            else score_diff = score_later <= score_before ? 0 : score_later - score_before;
            if(score_diff < min_score_diff) {
                min_score_diff = score_diff;
                max_dist_num = io.edge_dist_num[edge_idx];
                choose_now = edge_idx;
            } else if(score_diff == min_score_diff) {
                if(io.edge_dist_num[edge_idx] > max_dist_num) {
                    max_dist_num = io.edge_dist_num[edge_idx];
                    choose_now = edge_idx;
                }
            }
            // score_diff_sort.push_back({score_diff, io.edge_dist_num[edge_idx], edge_idx});
            // score_diff_sort.push_back({score_diff, io.sb_map[io.edges_names[edge_idx]] - sb_map_ref[io.edges_names[edge_idx]], edge_idx});
        }
        // std::sort(score_diff_sort.begin(), score_diff_sort.end(),
        //                   [](const std::tuple<int, int, int>& i,                    
        //                      const std::tuple<int, int, int>& j) { 
        //                          if(std::get<0>(i) == std::get<0>(j)) {
        //                              return std::get<1>(i) > std::get<1>(j);
        //                          }
        //                          else return i < j; });
        // if (score_diff_sort.size() == 0) return -1;
        // else return std::get<2>(score_diff_sort[0]);
        return choose_now;
    }


    int calculate_best_edge_with_edge_limit_with_choose_edge(int time, int cli_idx, int stream) {
        auto& sb_map_ref = sb_map_alltime[time];
        auto edges_relate = io.qos_map[cli_idx];
        std::vector<std::tuple<int, int, int>> score_diff_sort; // score_diff / edge_dist_num / edge_id 
        
        for(int i=0; i < edges_relate.size(); i++) {
            int edge_idx = edges_relate[i];
            if(!edge_choosed_and_sort_by_dist_num_bitmap[edge_idx]) continue;
            int score_diff = 0;
            int dist_max_before = edge_94_dist[edge_idx];
            int dist_later =  io.sb_map[io.edges_names[edge_idx]] - sb_map_ref[io.edges_names[edge_idx]] + stream;
            if (dist_later > io.sb_map[io.edges_names[edge_idx]] || dist_later > edges_avg_limit[edge_idx]) continue;
            int score_before = 0;
            int score_later = 0;

            
            if (dist_max_before == 0) {
                score_before += 0;
            } else if (dist_max_before <= io.base_cost) {
                score_before += io.base_cost;
            } else {
                int64_t over = dist_max_before - io.base_cost;
                score_before += dist_max_before;
                score_before += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
            }

            bool is_in_5_percent = false;
            for (auto time_5 : edge_5_percent_time[edge_idx]) {
                if(time == time_5) is_in_5_percent = true;
            }
            if(is_in_5_percent) score_later = -1;
            else if (dist_later == 0) {
                score_later = 0;
            } else if (dist_later <= io.base_cost) {
                score_later = io.base_cost;
            } else {
                int64_t over = dist_later - io.base_cost;
                score_later += dist_later;
                score_later += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
            }
            score_diff = score_later <= score_before ? 0 : score_later - score_before;
            score_diff_sort.push_back({score_diff, io.edge_dist_num[edge_idx], edge_idx});
        }
        std::sort(score_diff_sort.begin(), score_diff_sort.end(),
                          [](const std::tuple<int, int, int>& i,                    
                             const std::tuple<int, int, int>& j) { 
                                //  if(std::get<0>(i) == std::get<0>(j)) {
                                //      if(std::get<0>(i) >= 0) return std::get<1>(i) > std::get<1>(j);
                                //      else return std::get<1>(i) < std::get<1>(j);
                                //  }
                                //  else 
                                 return i < j; });
        if (score_diff_sort.size() == 0) return -1;
        else return std::get<2>(score_diff_sort[0]);
    }


    // 对计算得到的res进行重分配优化，有bug还调不出来，没什么用
    void res_redist() {
        int pos_94 = 0.05 * res.size();
        std::vector<int> edge_score_94(io.edges_names.size());
        std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
            io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));
        // std::vector<int> same_94_num(io.edges_names.size());
        for (int time = 0; time < res.size(); time++) {
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                sb_map_alltime[time][io.edges_names[edge_idx]];
                edge_sort_score[edge_idx][time] = {edge_dist, time};
            }
        }
        auto edge_sort_score_time_order = edge_sort_score;
        for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
            auto edge_score = edge_sort_score_time_order[edge_idx];
            std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
            edge_score_94[edge_idx] = edge_score[pos_94].first;
            // int same_94 = 0;
            // int pos = pos_94;
            // while (true) {
            //     if (edge_score_94[edge_idx] == edge_score[pos].first) {
            //         same_94++;
            //     } else
            //         break;
            //     if (pos == res.size() - 1) break;
            //     pos++;
            // }
            // same_94_num[edge_idx] = same_94;
            // std::cout << "edge_idx: " << edge_idx << " score: " << edge_score[pos_94].first << "
            // time: " << edge_score[pos_94].second << std::endl;
        }

        std::vector<std::pair<int, int>> sort_edge_dist_num;
        for (size_t i = 0; i < io.edges_names.size(); i++) {
            sort_edge_dist_num.push_back({io.edge_dist_num[i], i});
        }
        std::sort(sort_edge_dist_num.begin(), sort_edge_dist_num.end());  // 从小到大排列

        // int runtime = 100;
        int small_run_time = 10000;
        while (true) {
            bool hit = false;
            for (int k = 0; k < io.edges_names.size(); k++) {
                int edge_idx = sort_edge_dist_num[k].second;
                auto edge_score = edge_sort_score_time_order[edge_idx];
                std::sort(edge_score.begin(), edge_score.end(),
                          std::greater<std::pair<int, int>>());
                edge_score_94[edge_idx] = edge_score[pos_94].first;
                if (edge_score_94[edge_idx] <= io.base_cost) continue;
                std::set<int> envovled_clis;
                std::set<int> envovled_edges;
                for (int i = 0; i < io.edge_dist_clients[edge_idx].size(); i++) {
                    envovled_clis.insert(io.edge_dist_clients[edge_idx][i]);
                }
                for (auto cli_idx : envovled_clis) {
                    for (int j = 0; j < io.qos_map[cli_idx].size(); j++) {
                        envovled_edges.insert(io.qos_map[cli_idx][j]);
                    }
                }
                int sort_idx = pos_94;
                while (true) {
                    int time_in = edge_score[sort_idx].second;
                    for (auto edge_other : envovled_edges) {
                        if(edge_other == edge_idx) continue;
                        if (edge_score_94[edge_other] == 0) continue;
                        bool get = false;
                        std::set<int> clis_other;
                        std::set<int> avaliable_clis;
                        for (int i = 0; i < io.edge_dist_clients[edge_other].size(); i++) {
                            clis_other.insert(io.edge_dist_clients[edge_other][i]);
                        }
                        std::set_intersection(
                            envovled_clis.begin(), envovled_clis.end(), clis_other.begin(),
                            clis_other.end(),
                            std::inserter(avaliable_clis, avaliable_clis.begin()));

                        for (auto cli_idx : avaliable_clis) {
                            if (res[time_in][cli_idx][edge_idx].size() == 0) continue;
                            int diff_to_94 = edge_score_94[edge_other] -
                                             edge_sort_score_time_order[edge_other][time_in].first;
                            // if (diff_to_94 == 0) continue;
                            auto traverse = res[time_in][cli_idx][edge_idx].begin();
                            // std::sort(res[time_in][cli_idx][edge_idx].begin(),
                            // res[time_in][cli_idx][edge_idx].end());
                            while (traverse != res[time_in][cli_idx][edge_idx].end()) {
                                if (diff_to_94 > 0) {
                                    // 这种情况靠近%94
                                    int dist_v = (*traverse).first;
                                    if (dist_v == 0) {
                                        traverse++;
                                        continue;
                                    }
                                    // dist_v = std::min(diff_to_94, dist_v);
                                    if (diff_to_94 >= dist_v) {
                                        res[time_in][cli_idx][edge_other].push_back(
                                            {(*traverse).first, (*traverse).second});
                                        traverse = res[time_in][cli_idx][edge_idx].erase(traverse);
                                        edge_sort_score_time_order[edge_other][time_in].first =
                                            edge_sort_score_time_order[edge_other][time_in].first +
                                            dist_v;
                                        edge_sort_score_time_order[edge_idx][time_in].first =
                                            edge_sort_score_time_order[edge_idx][time_in].first -
                                            dist_v;
                                        sb_map_alltime[time_in][io.edges_names[edge_idx]] += dist_v;
                                        sb_map_alltime[time_in][io.edges_names[edge_other]] -= dist_v;
                                        diff_to_94 -= dist_v;
                                        small_run_time--;
                                        // std::cout << small_run_time << " ";
                                        if (small_run_time == 0) return;
                                        get = true;
                                        break;
                                        // std::cout << small_run_time << std::endl;
                                    } else {
                                        traverse++;
                                        continue;
                                        // break;
                                    }
                                } else if (diff_to_94 < 0) {
                                    // 这种情况靠近带宽上限
                                    int diff_to_sb =
                                        io.sb_map[io.edges_names[edge_other]] -
                                        edge_sort_score_time_order[edge_other][time_in].first;
                                    // int dist_v = std::min(res[time_in][cli_idx][edge_idx],
                                    // res[time_in][cli_idx][edge_other]);
                                    int dist_v = (*traverse).first;
                                    if (dist_v == 0) {
                                        traverse++;
                                        continue;
                                    }
                                    if (diff_to_sb >= dist_v) {
                                        res[time_in][cli_idx][edge_other].push_back(
                                            {(*traverse).first, (*traverse).second});
                                        traverse = res[time_in][cli_idx][edge_idx].erase(traverse);
                                        edge_sort_score_time_order[edge_other][time_in].first =
                                            edge_sort_score_time_order[edge_other][time_in].first +
                                            dist_v;
                                        edge_sort_score_time_order[edge_idx][time_in].first =
                                            edge_sort_score_time_order[edge_idx][time_in].first -
                                            dist_v;
                                        sb_map_alltime[time_in][io.edges_names[edge_idx]] += dist_v;
                                        sb_map_alltime[time_in][io.edges_names[edge_other]] -= dist_v;
                                        diff_to_94 -= dist_v;
                                        small_run_time--;
                                        // std::cout << small_run_time << " ";
                                        if (small_run_time == 0) return;
                                        get = true;
                                        
                                        break;
                                        // std::cout << small_run_time << std::endl;
                                    } else {
                                        traverse++;
                                        continue;
                                        // break;
                                    }
                                } else {
                                    // traverse++;
                                    // int dist_v = (*traverse).first;
                                    // int diff_other = calculate_score_diff(edge_other, edge_score_94[edge_other], edge_score_94[edge_other] + dist_v);
                                    // int diff_this = calculate_score_diff(edge_other, edge_score_94[edge_idx] - dist_v, edge_score_94[edge_other]);
                                    // if(diff_other < diff_this){
                                    //     res[time_in][cli_idx][edge_other].push_back(
                                    //         {(*traverse).first, (*traverse).second});
                                    //     traverse = res[time_in][cli_idx][edge_idx].erase(traverse);
                                    //     edge_sort_score_time_order[edge_other][time_in].first =
                                    //         edge_sort_score_time_order[edge_other][time_in].first +
                                    //         dist_v;
                                    //     edge_sort_score_time_order[edge_idx][time_in].first =
                                    //         edge_sort_score_time_order[edge_idx][time_in].first -
                                    //         dist_v;
                                    //     sb_map_alltime[time_in][io.edges_names[edge_idx]] += dist_v;
                                    //     sb_map_alltime[time_in][io.edges_names[edge_other]] -= dist_v;
                                    //     diff_to_94 -= dist_v;
                                    //     break;
                                    // } else {
                                    //     traverse++;
                                    //     continue;
                                    // }
                                    break;
                                }
                            }
                            // if(get) break;
                        }
                        // if(get) break;
                    }
                    // if (sort_idx == res.size() - 1 || sort_idx - pos_94 >= 10) {
                    //     break;
                    // } else {
                    //     if (edge_sort_score_time_order[edge_idx][time_in].first <
                    //         edge_sort_score_time_order[edge_idx][edge_score[sort_idx + 1].second]
                    //             .first) {
                    //         edge_score_94[edge_idx] =
                    //             edge_sort_score_time_order[edge_idx]
                    //                                       [edge_score[sort_idx + 1].second]
                    //                                           .first;
                    //         sort_idx++;
                    //         hit = true;
                    //     } else {
                    //         if (edge_sort_score_time_order[edge_idx][time_in].first <
                    //             edge_score_94[edge_idx])
                    //             hit = true;
                    //         edge_score_94[edge_idx] =
                    //             edge_sort_score_time_order[edge_idx][time_in].first;
                    //         break;
                    //     }
                    // }
                    if (sort_idx == res.size() - 1) break;
                    else {
                        if (edge_sort_score_time_order[edge_idx][time_in].first <
                            edge_sort_score_time_order[edge_idx][edge_score[sort_idx + 1].second]
                                .first) {
                            edge_score_94[edge_idx] =
                                edge_sort_score_time_order[edge_idx]
                                                            [edge_score[sort_idx + 1].second]
                                                                .first;
                            sort_idx++;
                            hit = true;
                        } else {
                            if (edge_sort_score_time_order[edge_idx][time_in].first <
                                edge_score_94[edge_idx])
                                hit = true;
                            edge_score_94[edge_idx] =
                                edge_sort_score_time_order[edge_idx][time_in].first;
                            break;
                        }
                    }
                    // break;
                    
                }
                edge_score = edge_sort_score_time_order[edge_idx];
                std::sort(edge_score.begin(), edge_score.end(),
                          std::greater<std::pair<int, int>>());
                edge_score_94[edge_idx] = edge_score[pos_94].first;
            }
            // runtime--;
            // if (!runtime) break;
            if (!hit) break;
        }
        // std::cout << "RUNTIME:" << runtime << std::endl;
    }

    int calculate_score_diff(int edge_idx, int dist_max_before, int dist_max_later) {
        int score_before = 0;
        int score_later = 0;

        
        if (dist_max_before == 0) {
            score_before += 0;
        } else if (dist_max_before <= io.base_cost) {
            score_before += io.base_cost;
        } else {
            int64_t over = dist_max_before - io.base_cost;
            score_before += dist_max_before;
            score_before += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
        }

        if (dist_max_later == 0) {
            score_later = 0;
        } else if (dist_max_later <= io.base_cost) {
            score_later = io.base_cost;
        } else {
            int64_t over = dist_max_later - io.base_cost;
            score_later += dist_max_later;
            score_later += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
        }
        return score_later - score_before;
    }

    /* 计分函数 */
    int calculate_94_score() {
        int pos_94 = 0.05 * res.size();
        std::vector<int> edge_score_94(io.edges_names.size());
        std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
            io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

        for (int time = 0; time < res.size(); time++) {
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                sb_map_alltime[time][io.edges_names[edge_idx]];
                edge_sort_score[edge_idx][time] = {edge_dist, time};
            }
        }
        double sum = 0;
        for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
            auto edge_score = edge_sort_score[edge_idx];
            std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
            edge_score_94[edge_idx] = edge_score[pos_94].first;
            if (edge_score[0].first == 0) {
                sum += 0;
            } else if (edge_score_94[edge_idx] <= io.base_cost) {
                sum += io.base_cost;
            } else {
                int64_t over = edge_score_94[edge_idx] - io.base_cost;
                sum += edge_score_94[edge_idx];
                sum += double(over * over) / io.sb_map[io.edges_names[edge_idx]];
            }
            // sum += edge_score_94[edge_idx];
            // std::cout << "edge_idx: " << edge_idx << " score: " << edge_score[pos_94].first
            // << " time: " << edge_score[pos_94].second << std::endl;
        }
        sum += 0.5;
        return sum;
    }

    /* 计算边缘节点上限 */
    void calculate_each_edge_avg_upper_bound(int cost_limit) {
        edges_avg_limit.resize(io.edges_names.size());
        // std::vector<int> cli_avg(io.client_names.size());
        // std::vector<int> edges_cost_limit(io.edges_names.size());
        // for(int cli_idx=0; cli_idx < io.client_names.size(); cli_idx++) {

        // }
        // for(int edge_idx=0; edge_idx<io.edges_names.size(); edge_idx++) {

        // }
        for (int i = 0; i < io.edges_names.size(); i++) {
            int64_t c = io.sb_map[io.edges_names[i]];
            int64_t v = io.base_cost;
            edges_avg_limit[i] = (2 * v - c + sqrt(c * c - 4 * c * v + 4 * cost_limit * c)) / 2;
        }
        // test
        //  for (auto limit : edges_avg_limit) std::cout << limit << " ";
        //  std::cout << std::endl;
        //  return;
    }

    /* 计算边缘节点上限 */
    void calculate_each_edge_avg_dist_limit_with_input_rest_dist(double avg_coefficient, double rest_dist) {
        edges_avg_limit.resize(io.edges_names.size(), 0);
        std::vector<int> clients_max_require(io.client_names.size(), 0);
        std::vector<int> edge_max_clients_require(io.edges_names.size(), 0);
        int edge_max_clients_require_sum = 0;
        int clients_max_require_sum = 0;
        for (int cli_idx = 0; cli_idx < io.client_names.size(); cli_idx++) {
            int max_require = 0;
            for(int time = 0; time < io.data_dm_rowstore.size(); time++) {
                int require = 0;
                for(auto& stream: io.data_dm_rowstore[time][cli_idx]) {
                    require += stream.first;
                }
                max_require = std::max(require, max_require);
            }
            clients_max_require[cli_idx] = max_require;
            clients_max_require_sum += max_require;
        }

        // std::cout << "DIFF: " << clients_max_require_sum - rest_dist << std::endl;

        for(int edge_idx=0; edge_idx<io.edges_names.size(); edge_idx++) {
            auto& clis = io.edge_dist_clients[edge_idx];
            for(auto cli : clis) {
                edge_max_clients_require[edge_idx] += clients_max_require[cli];
            }
            edge_max_clients_require_sum += edge_max_clients_require[edge_idx];
        }
        double edge_max_clients_require_split = rest_dist / edge_max_clients_require_sum;
        // double edge_max_clients_require_split = rest_dist / io.edges_names.size();
        for (int i = 0; i < io.edges_names.size(); i++) {
            int64_t c = io.sb_map[io.edges_names[i]];
            int64_t v = io.base_cost;
            int64_t cost_limit = edge_max_clients_require_split * edge_max_clients_require[i] * avg_coefficient;
            // int64_t cost_limit = edge_max_clients_require_split * avg_coefficient;
            edges_avg_limit[i] = (2*v - c + sqrt(c*c - 4*c*v + 4*cost_limit*c)) / 2;
        }
        // test
        // for (auto limit : edges_avg_limit) std::cout << limit << " ";
        // std::cout << std::endl;
        return;
    }

    void calculate_each_edge_avg_dist_limit(double avg_coefficient) {
        edges_avg_limit.resize(io.edges_names.size(), 0);
        std::vector<int> clients_max_require(io.client_names.size(), 0);
        std::vector<int> edge_max_clients_require(io.edges_names.size(), 0);
        double edge_max_clients_require_sum = 0;
        double rest_dist = 0;
        for (int cli_idx = 0; cli_idx < io.client_names.size(); cli_idx++) {
            int max_require = 0;
            for(int time = 0; time < io.data_dm_rowstore.size(); time++) {
                int require = 0;
                for(auto& stream: io.data_dm_rowstore[time][cli_idx]) {
                    require += stream.first;
                }
                max_require = std::max(require, max_require);
            }
            clients_max_require[cli_idx] = max_require;
            rest_dist += max_require;
        }

        for(int edge_idx=0; edge_idx<io.edges_names.size(); edge_idx++) {
            auto& clis = io.edge_dist_clients[edge_idx];
            for(auto cli : clis) {
                edge_max_clients_require[edge_idx] += clients_max_require[cli];
            }
            edge_max_clients_require_sum += edge_max_clients_require[edge_idx];
        }
        double edge_max_clients_require_split = rest_dist / edge_max_clients_require_sum;
        // double edge_max_clients_require_split = rest_dist / io.edges_names.size();
        for (int i = 0; i < io.edges_names.size(); i++) {
            int64_t c = io.sb_map[io.edges_names[i]];
            int64_t v = io.base_cost;
            int64_t cost_limit = edge_max_clients_require_split * edge_max_clients_require[i] * avg_coefficient;
            // int64_t cost_limit = edge_max_clients_require_split * avg_coefficient;
            // std::cout << cost_limit << " ";
            edges_avg_limit[i] = (2*v - c + sqrt(c*c - 4*c*v + 4*cost_limit*c)) / 2;
            
        }
        // test
        // for (auto limit : edges_avg_limit) std::cout << limit << " ";
        // std::cout << std::endl;
        return;
    }

    void output_edge_dist() {
        std::ofstream fout("./output/edge_dist.txt", std::ios_base::trunc);
        int pos_94 = 0.05 * res.size();
        std::vector<int> edge_score_94(io.edges_names.size());
        std::vector<std::vector<std::pair<int, int>>> edge_sort_score(
            io.edges_names.size(), std::vector<std::pair<int, int>>(res.size()));

        for (int time = 0; time < res.size(); time++) {
            for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
                int edge_dist = io.sb_map[io.edges_names[edge_idx]] -
                                sb_map_alltime[time][io.edges_names[edge_idx]];
                edge_sort_score[edge_idx][time] = {edge_dist, time};
            }
        }
        for (int edge_idx = 0; edge_idx < io.edges_names.size(); edge_idx++) {
            fout << "EDGE: " << edge_idx << std::endl;
            auto edge_score = edge_sort_score[edge_idx];
            std::sort(edge_score.begin(), edge_score.end(), std::greater<std::pair<int, int>>());
            for(int i=0; i<edge_score.size(); i++) {
                // fout << "DIST:" << edge_score[i].first << "TIME:" << edge_score[i].second << " ";
                fout << edge_score[i].first << " ";
            }
            fout << std::endl;
            fout << std::endl;
        }
        fout.close();
    }
    /* 处理输出 */
    void handle_output() { io.handle_output(res); }

   public:
    ContestIO io;
    std::vector<std::unordered_map<std::string, int>> sb_map_alltime;
    // 时间 / 客户端 / 边缘节点 / 对当前客户端那些流进行了分配
    std::vector<std::vector<std::vector<std::vector<std::pair<int, std::string>>>>> res;
    // 每个节点平均分配的上限
    std::vector<int64_t> edges_avg_limit;
    std::vector<int> edge_94_dist;
    std::vector<std::vector<int>> edge_5_percent_time;
    std::vector<bool> edge_choosed_and_sort_by_dist_num_bitmap;
    std::vector<int> edge_choosed_and_sort_by_dist_num;
    std::vector<bool> edge_choosed_and_sort_by_dist_num_first_round_bitmap;
    std::vector<int> edge_choosed_and_sort_by_dist_first_round_num;
};
