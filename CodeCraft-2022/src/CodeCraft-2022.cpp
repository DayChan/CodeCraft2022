#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
// using namespace std;

// 提交的时候要注释掉这个宏
// #define DEBUG
void handle_csv(std::ifstream& fp,
                std::unordered_map<std::string, std::vector<std::string>>& output,
                std::vector<std::string>& columns) {
    std::string line;
    getline(fp, line);  //列名
    if (line.size() && line[line.size() - 1] == '\r') {
        line = line.substr(0, line.size() - 1);
    }
    std::istringstream readstr(line);
    std::string elem;

    while (getline(readstr, elem, ',')) {
        columns.push_back(elem);
    }

    while (getline(fp, line)) {
        if (line.size() && line[line.size() - 1] == '\r') {
            line = line.substr(0, line.size() - 1);
        }
        std::vector<std::string> data_line;
        std::string elem;
        std::istringstream readstr(line);
        //将一行数据按'，'分割
        int idx = 0;
        while (getline(readstr, elem, ',')) {
            output[columns[idx++]].push_back(elem);
        }
    }
}

void handle_dm(std::unordered_map<std::string, std::vector<std::string>>& data_dm,
               std::vector<std::string>& cols_client,
               std::vector<std::vector<int>>& data_dm_rowstore) {
    size_t len = data_dm["mtime"].size();
    for (size_t i = 0; i < len; i++) {
        std::vector<int> row;
        for (size_t j = 0; j < cols_client.size(); j++) {
            row.push_back(atoi(data_dm[cols_client[j]][i].c_str()));
        }
        data_dm_rowstore.emplace_back(std::move(row));
    }
}

// void handle_qos(std::unordered_map<std::string ,std::vector<std::string>>& data_qos,
// std::vector<std::string>& cols_qos, std::unordered_map<std::string,
// std::unordered_map<std::string, int>>& output, std::vector<std::string>& cols_edges) {
//     for(size_t i=0; i < data_qos["site_name"].size(); i++) {
//         for(size_t j=1; j<cols_qos.size();j++) {
//             output[data_qos["site_name"][i]][cols_qos[j]] =
//             atoi(data_qos[cols_qos[j]][i].c_str());
//         }
//     }
//     cols_edges = data_qos["site_name"];
// }

void handle_qos_filter(std::unordered_map<std::string, std::vector<std::string>>& data_qos,
                       std::vector<std::string>& cols_client, std::vector<std::vector<int>>& output,
                       std::vector<int>& edge_dist_num, std::vector<std::vector<int>>& edge_dist_clients,
                       std::vector<std::string>& cols_edges,
                       int qos_constrain) {
    cols_edges = data_qos["site_name"];
    edge_dist_num.resize(cols_edges.size(), 0);
    edge_dist_clients.resize(cols_edges.size());
    for (size_t i = 0; i < cols_client.size(); i++) {
        for (size_t j = 0; j < data_qos["site_name"].size(); j++) {
            if (atoi(data_qos[cols_client[i]][j].c_str()) < qos_constrain) {
                output[i].push_back(j);
                edge_dist_num[j]++;
                edge_dist_clients[j].push_back(i);
            }
        }
    }
}

void handle_sb(std::unordered_map<std::string, std::vector<std::string>>& data_sb,
               std::unordered_map<std::string, int>& sb_map) {
    std::vector<std::string> sn = data_sb["site_name"];
    std::vector<std::string> bw = data_sb["bandwidth"];
    for (size_t i = 0; i < sn.size(); i++) {
        sb_map[sn[i]] = atoi(bw[i].c_str());
    }
}

void handle_contest_input(std::vector<std::string>& cols_client,
                          std::vector<std::string>& cols_edges,
                          std::vector<std::vector<int>>& data_dm_rowstore,
                          std::vector<std::vector<int>>& qos_map, std::vector<int>& edge_dist_num,
                          std::vector<std::vector<int>>& edge_dist_clients,
                          std::unordered_map<std::string, int>& sb_map, int& qos_constrain) {
#ifdef DEBUG
    std::ifstream fp_cfg("./data/config.ini");
#else
    std::ifstream fp_cfg("/data/config.ini");
#endif
    std::string cfg_line;
    getline(fp_cfg, cfg_line);
    getline(fp_cfg, cfg_line);  // 获取第二行
    if (cfg_line.size() && cfg_line[cfg_line.size() - 1] == '\r') {
        cfg_line = cfg_line.substr(0, cfg_line.size() - 1);
    }
    std::istringstream cfg_line_iss(cfg_line);
    std::string elem;
    getline(cfg_line_iss, elem, '=');
    getline(cfg_line_iss, elem, '=');  // 获取qos_constrain;
    qos_constrain = atoi(elem.c_str());

    std::vector<std::vector<std::string>> user_arr;
#ifdef DEBUG
    std::ifstream fp_dm("./data/demand.csv");  //定义声明一个ifstream对象，指定文件路径
#else
    std::ifstream fp_dm("/data/demand.csv");  //定义声明一个ifstream对象，指定文件路径
#endif
    std::unordered_map<std::string, std::vector<std::string>> data_dm;
    handle_csv(fp_dm, data_dm, cols_client);
    cols_client.erase(cols_client.begin());  // 删除首位mtime
    handle_dm(data_dm, cols_client, data_dm_rowstore);

#ifdef DEBUG
    std::ifstream fp_qos("./data/qos.csv");
#else
    std::ifstream fp_qos("/data/qos.csv");
#endif
    std::unordered_map<std::string, std::vector<std::string>> data_qos;
    std::vector<std::string> cols_qos;
    qos_map.resize(cols_client.size());
    handle_csv(fp_qos, data_qos, cols_qos);
    handle_qos_filter(data_qos, cols_client, qos_map, edge_dist_num, edge_dist_clients, cols_edges, qos_constrain);
    // std::cout << "size: " << qos_map[0].size() << std::endl;
    // for(size_t i=0; i<qos_map[0].size(); i++) {
    //     std::cout << cols_edges[qos_map[0][i]] << " ";
    // }
    // std::cout << std::endl;
    // std::cout << edge_dist_num[99] << std::endl;

#ifdef DEBUG
    std::ifstream fp_sb("./data/site_bandwidth.csv");
#else
    std::ifstream fp_sb("/data/site_bandwidth.csv");
#endif
    std::unordered_map<std::string, std::vector<std::string>> data_sb;
    std::vector<std::string> cols_sb;
    handle_csv(fp_sb, data_sb, cols_sb);
    handle_sb(data_sb, sb_map);
}

void contest_calculate_avg(std::vector<std::string>& client_names,
                           std::vector<std::string>& edges_names,
                           std::vector<std::vector<int>>& data_dm_rowstore,
                           std::vector<std::vector<int>>& qos_map, std::vector<int>& edge_dist_num,
                           std::unordered_map<std::string, int>& sb_map, int qos_constrain,
                           std::vector<std::vector<std::vector<int>>>& res) {
    // //
    // std::vector<std::string> res; // 输出的每一行
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    int split_max = 1000;
    for (size_t time = 0; time < data_dm_rowstore.size(); time++) {
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];
            while (cli_dm) {
                for (size_t i = 0; i < qos_map[cli_idx].size() && cli_dm; i++) {
                    int edge_idx = qos_map[cli_idx][i];
                    int dist_v = split_max / edge_dist_num[edge_idx];
                    int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                    dist_v = std::min(dist_v, cli_dm);
                    dist_v = std::min(dist_v, edge_rest_sb);
                    cli_dm -= dist_v;
                    edge_rest_sb -= dist_v;
                    res[time][cli_idx][edge_idx] += dist_v;
                }
            }
        }
    }
    // auto output0 = res[0];
    // for(size_t cli_idx=0; cli_idx < output0.size(); cli_idx++) {
    //     std::cout << client_names[cli_idx] << ":";
    //     int sum=0;
    //     for(size_t edge_idx=0; edge_idx < output0[cli_idx].size(); edge_idx++) {
    //         if(output0[cli_idx][edge_idx] != 0) {
    //             std::cout << "<" << edges_names[edge_idx] << "," << output0[cli_idx][edge_idx] <<
    //             ">,"; sum += output0[cli_idx][edge_idx];
    //         }
    //     }
    //     std::cout << " sum: " << sum;
    //     std::cout << std::endl;
    // }
    return;
}

void contest_calculate_95_attack1(std::vector<std::string>& client_names,
                                  std::vector<std::string>& edges_names,
                                  std::vector<std::vector<int>>& data_dm_rowstore,
                                  std::vector<std::vector<int>>& qos_map,
                                  std::vector<int>& edge_dist_num,
                                  std::unordered_map<std::string, int>& sb_map, int qos_constrain,
                                  std::vector<std::vector<std::vector<int>>>& res) {
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    int split_max = 1000;
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    for (size_t time = 0; time < data_dm_rowstore.size(); time++) {
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];
            for (size_t i = 0; i < qos_map[cli_idx].size() && cli_dm; i++) {
                int edge_idx = qos_map[cli_idx][i];
                if (edge_times[edge_idx] + edge_used[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }
            while (cli_dm) {
                for (size_t i = 0; i < qos_map[cli_idx].size() && cli_dm; i++) {
                    int edge_idx = qos_map[cli_idx][i];
                    int dist_v = split_max / edge_dist_num[edge_idx];
                    int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                    dist_v = std::min(dist_v, cli_dm);
                    dist_v = std::min(dist_v, edge_rest_sb);
                    cli_dm -= dist_v;
                    edge_rest_sb -= dist_v;
                    res[time][cli_idx][edge_idx] += dist_v;
                }
            }
        }
        for (size_t i = 0; i < edge_times.size(); i++) edge_times[i] += edge_used[i];
    }
    return;
}

void contest_calculate_95_attack2(std::vector<std::string>& client_names,
                                  std::vector<std::string>& edges_names,
                                  std::vector<std::vector<int>>& data_dm_rowstore,
                                  std::vector<std::vector<int>>& qos_map,
                                  std::vector<int>& edge_dist_num,
                                  std::unordered_map<std::string, int>& sb_map, int qos_constrain,
                                  std::vector<std::vector<std::vector<int>>>& res) {
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    int split_max = 1000;
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    for (size_t time = 0; time < data_dm_rowstore.size(); time++) {
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];
            for (size_t i = 0; i < qos_map[cli_idx].size() && cli_dm; i++) {
                int edge_idx = qos_map[cli_idx][i];
                if (edge_times[edge_idx] + edge_used[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            while (cli_dm) {
                int dist_v = cli_dm / qos_map[cli_idx].size() + 1;
                for (size_t i = 0; i < qos_map[cli_idx].size() && cli_dm; i++) {
                    int edge_idx = qos_map[cli_idx][i];
                    int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                    int dist_v_final = std::min(dist_v, cli_dm);
                    dist_v_final = std::min(dist_v_final, edge_rest_sb);
                    cli_dm -= dist_v_final;
                    edge_rest_sb -= dist_v_final;
                    res[time][cli_idx][edge_idx] += dist_v_final;
                }
            }
        }
        for (size_t i = 0; i < edge_times.size(); i++) edge_times[i] += edge_used[i];
    }
    return;
}

void contest_calculate_95_attack3(std::vector<std::string>& client_names,
                                  std::vector<std::string>& edges_names,
                                  std::vector<std::vector<int>>& data_dm_rowstore,
                                  std::vector<std::vector<int>>& qos_map,
                                  std::vector<int>& edge_dist_num,
                                  std::unordered_map<std::string, int>& sb_map, int qos_constrain,
                                  std::vector<std::vector<std::vector<int>>>& res) {
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    int split_max = 1000;
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    for (size_t time = 0; time < data_dm_rowstore.size(); time++) {
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];
            for (size_t i = 0; i < qos_map[cli_idx].size() && cli_dm; i++) {
                int edge_idx = qos_map[cli_idx][i];
                if (edge_times[edge_idx] + edge_used[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            for (size_t i = 0; i < qos_map[cli_idx].size() && cli_dm; i++) {
                int edge_idx = qos_map[cli_idx][i];
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }
        }
        for (size_t i = 0; i < edge_times.size(); i++) edge_times[i] += edge_used[i];
    }
    return;
}

void contest_calculate_95_attack4(std::vector<std::string>& client_names,
                                  std::vector<std::string>& edges_names,
                                  std::vector<std::vector<int>>& data_dm_rowstore,
                                  std::vector<std::vector<int>>& qos_map,
                                  std::vector<int>& edge_dist_num,
                                  std::unordered_map<std::string, int>& sb_map, int qos_constrain,
                                  std::vector<std::vector<std::vector<int>>>& res) {
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    for (size_t time = 0; time < data_dm_rowstore.size(); time++) {
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            // 对单个时间内已经分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time || edge_used[edge_idx] != 1) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 对单个时间内未分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 使用已经超分配过的edge进行分配
            for (size_t i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (!edge_exceed[edge_idx]) continue;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }
            // 使用未超分配过的edge进行分配

            for (size_t i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_exceed[edge_idx]) continue;
                edge_exceed[edge_idx] = true;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }
        }
        for (size_t i = 0; i < edge_times.size(); i++) edge_times[i] += edge_used[i];
    }
    return;
}

void contest_calculate_95_with_maxrecord(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    for (size_t time = 0; time < data_dm_rowstore.size(); time++) {
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        std::vector<int> edge_record_acm(edges_names.size(), 0);  // edge当前轮次累计
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            // 对单个时间内已经分配过的edge进行分配
            for (int i = sort_array.size() - 1; i >= 0 && cli_dm; i--) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time || edge_used[edge_idx] != 1) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 对单个时间内未分配过的edge进行分配
            for (int i = sort_array.size() - 1; i >= 0 && cli_dm; i--) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 经过以上所有edge节点已经分配超过5%次数

            // 先分配带宽剩余最多的，还是先分配之前单轮次分配带宽最多的？
            // 单轮次分配带宽最大记录排序
            int dist_v = cli_dm / qos_map[cli_idx].size();

            std::vector<std::pair<int, int>> max_record_sort;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                max_record_sort.push_back(
                    {edge_max_record[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(max_record_sort.begin(), max_record_sort.end(),
                      std::greater<std::pair<int, int>>());

            for (size_t i = 0; i < max_record_sort.size() && cli_dm; i++) {
                int edge_idx = max_record_sort[i].second;
                int rest_to_max = edge_max_record[i] - edge_record_acm[edge_idx];
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v_in = std::min(dist_v_in, rest_to_max);
                dist_v_in = std::min(dist_v_in, edge_rest_sb);
                dist_v_in = std::min(dist_v_in, cli_dm);
                cli_dm -= dist_v_in;
                edge_rest_sb -= dist_v_in;
                res[time][cli_idx][edge_idx] += dist_v_in;
                edge_record_acm[edge_idx] += dist_v_in;
            }

            // 余量排序
            std::vector<std::pair<int, int>> sb_rest_sort;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sb_rest_sort.push_back(
                    {sb_map_cp[edges_names[qos_map[cli_idx][i]]], qos_map[cli_idx][i]});
            }
            std::sort(sb_rest_sort.begin(), sb_rest_sort.end(),
                      std::greater<std::pair<int, int>>());

            while (cli_dm) {
                for (int i = 0; i < sb_rest_sort.size() && cli_dm; i++) {
                    int edge_idx = sb_rest_sort[i].second;
                    int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                    int dist_v_in = dist_v;
                    dist_v_in = std::min(dist_v_in, edge_rest_sb);
                    dist_v_in = std::min(dist_v_in, cli_dm);
                    cli_dm -= dist_v_in;
                    edge_rest_sb -= dist_v_in;
                    res[time][cli_idx][edge_idx] += dist_v_in;
                    edge_record_acm[edge_idx] += dist_v_in;
                    edge_max_record[edge_idx] =
                        std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                }
            }

            if (cli_dm != 0) std::cout << cli_dm << std::endl;
        }
        for (size_t i = 0; i < edge_times.size(); i++) {
            edge_times[i] += edge_used[i];
            edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
        }
    }
    return;
}

void contest_calculate_95_with_rank(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    int base = 100;
    for (size_t time = 0; time < data_dm_rowstore.size(); time++) {
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        std::vector<int> edge_record_acm(edges_names.size(), 0);  // edge当前轮次累计
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            // 对单个时间内已经分配过的edge进行分配
            for (int i = sort_array.size() - 1; i >= 0 && cli_dm; i--) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time || edge_used[edge_idx] != 1) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 对单个时间内未分配过的edge进行分配
            for (int i = sort_array.size() - 1; i >= 0 && cli_dm; i--) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 经过以上所有edge节点已经分配超过5%次数

            // 先分配带宽剩余最多的，还是先分配之前单轮次分配带宽最多的？
            // 单轮次分配带宽最大记录排序
            std::vector<std::pair<int, int>> max_record_sort;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                max_record_sort.push_back(
                    {edge_max_record[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(max_record_sort.begin(), max_record_sort.end(),
                      std::greater<std::pair<int, int>>());

            // 根据评分分配
            while(cli_dm) {
                for (size_t i = 0; i < max_record_sort.size() && cli_dm; i++) {
                    int edge_idx = max_record_sort[i].second;
                    double score = double(edge_dist_num[edge_idx]) / max_record_sort.size();
                    int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                    int dist_v_in = score * base;
                    dist_v_in = dist_v_in * dist_v_in;
                    dist_v_in = std::min(dist_v_in, edge_rest_sb);
                    dist_v_in = std::min(dist_v_in, cli_dm);
                    cli_dm -= dist_v_in;
                    edge_rest_sb -= dist_v_in;
                    res[time][cli_idx][edge_idx] += dist_v_in;
                    edge_record_acm[edge_idx] += dist_v_in;
                }
            }
            
            if (cli_dm != 0) std::cout << cli_dm << std::endl;
        }
        for (size_t i = 0; i < edge_times.size(); i++) {
            edge_times[i] += edge_used[i];
            edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
        }
    }
    return;
}

void contest_calculate_95_with_maxrequire_sort1(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

    for(int i=0; i < data_dm_rowstore.size(); i++) {
        std::vector<int>& row = data_dm_rowstore[i];
        int sumrow = 0;
        for(auto num: row) sumrow += num;
        sort_time[i] = {sumrow, i};
    }

    std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

    for (size_t st = 0; st < sort_time.size(); st++) {
        int time = sort_time[st].second;
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        std::vector<int> edge_record_acm(edges_names.size(), 0);  // edge当前轮次累计
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            // 对单个时间内已经分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time || edge_used[edge_idx] != 1) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 对单个时间内未分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // int dist_v = cli_dm / qos_map[cli_idx].size();
            
            // std::vector<std::pair<int, int>> max_record_sort;
            // for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
            //     max_record_sort.push_back(
            //         {edge_max_record[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            // }
            // std::sort(max_record_sort.begin(), max_record_sort.end(),
            //           std::greater<std::pair<int, int>>());

            // 使用已经超分配过的edge进行分配
            for (size_t i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (!edge_exceed[edge_idx]) continue;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
                edge_record_acm[edge_idx] += dist_v;
                edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
            }

            // 使用未超分配过的edge进行分配
            for (size_t i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_exceed[edge_idx]) continue;
                edge_exceed[edge_idx] = true;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
                edge_record_acm[edge_idx] += dist_v;
            }

        }

        for (size_t i = 0; i < edge_times.size(); i++) {
            edge_times[i] += edge_used[i];
            edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
        }
    }
    return;
}


void contest_calculate_95_with_maxrequire_sort2(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

    for(int i=0; i < data_dm_rowstore.size(); i++) {
        std::vector<int>& row = data_dm_rowstore[i];
        int sumrow = 0;
        for(auto num: row) sumrow += num;
        sort_time[i] = {sumrow, i};
    }

    std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

    for (size_t st = 0; st < sort_time.size(); st++) {
        int time = sort_time[st].second;
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        std::vector<int> edge_record_acm(edges_names.size(), 0);  // edge当前轮次累计
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            // 对单个时间内已经分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time || edge_used[edge_idx] != 1) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 对单个时间内未分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // int dist_v = cli_dm / qos_map[cli_idx].size();
            
            std::vector<std::pair<int, int>> max_record_sort;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                max_record_sort.push_back(
                    {edge_max_record[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(max_record_sort.begin(), max_record_sort.end(),
                      std::greater<std::pair<int, int>>());

            // 使用已经超分配过的edge进行分配
            for (size_t i = 0; i < max_record_sort.size() && cli_dm; i++) {
                int edge_idx = max_record_sort[i].second;
                if (!edge_exceed[edge_idx]) continue;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
                edge_record_acm[edge_idx] += dist_v;
                edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
            }

            // 使用未超分配过的edge进行分配
            for (size_t i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_exceed[edge_idx]) continue;
                edge_exceed[edge_idx] = true;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
                edge_record_acm[edge_idx] += dist_v;
            }

        }

        for (size_t i = 0; i < edge_times.size(); i++) {
            edge_times[i] += edge_used[i];
            edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
        }
    }
    return;
}

void contest_calculate_95_with_maxrequire_sort3(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

    for(int i=0; i < data_dm_rowstore.size(); i++) {
        std::vector<int>& row = data_dm_rowstore[i];
        int sumrow = 0;
        for(auto num: row) sumrow += num;
        sort_time[i] = {sumrow, i};
    }

    std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

    for (size_t st = 0; st < sort_time.size(); st++) {
        int time = sort_time[st].second;
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        std::vector<int> edge_record_acm(edges_names.size(), 0);  // edge当前轮次累计
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            // 对单个时间内已经分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time || edge_used[edge_idx] != 1) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 对单个时间内未分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 经过以上所有edge节点已经分配超过5%次数

            // 先分配带宽剩余最多的，还是先分配之前单轮次分配带宽最多的？
            // 单轮次分配带宽最大记录排序
            int dist_v = cli_dm / qos_map[cli_idx].size();

            std::vector<std::pair<int, int>> max_record_sort;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                max_record_sort.push_back(
                    {edge_max_record[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(max_record_sort.begin(), max_record_sort.end(),
                      std::greater<std::pair<int, int>>());

            for (size_t i = 0; i < max_record_sort.size() && cli_dm; i++) {
                int edge_idx = max_record_sort[i].second;
                int rest_to_max = edge_max_record[i] - edge_record_acm[edge_idx];
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v_in = std::min(dist_v_in, rest_to_max);
                dist_v_in = std::min(dist_v_in, edge_rest_sb);
                dist_v_in = std::min(dist_v_in, cli_dm);
                cli_dm -= dist_v_in;
                edge_rest_sb -= dist_v_in;
                res[time][cli_idx][edge_idx] += dist_v_in;
                edge_record_acm[edge_idx] += dist_v_in;
            }

            // 余量排序
            // std::vector<std::pair<int, int>> sb_rest_sort;
            // for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
            //     sb_rest_sort.push_back(
            //         {sb_map_cp[edges_names[qos_map[cli_idx][i]]], qos_map[cli_idx][i]});
            // }
            // std::sort(sb_rest_sort.begin(), sb_rest_sort.end(),
            //           std::greater<std::pair<int, int>>());

            while (cli_dm) {
                for (int i = 0; i < max_record_sort.size() && cli_dm; i++) {
                    int edge_idx = max_record_sort[i].second;
                    int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                    int dist_v_in = dist_v;
                    dist_v_in = std::min(dist_v_in, edge_rest_sb);
                    dist_v_in = std::min(dist_v_in, cli_dm);
                    cli_dm -= dist_v_in;
                    edge_rest_sb -= dist_v_in;
                    res[time][cli_idx][edge_idx] += dist_v_in;
                    edge_record_acm[edge_idx] += dist_v_in;
                    edge_max_record[edge_idx] =
                        std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
                }
            }
        }

        for (size_t i = 0; i < edge_times.size(); i++) {
            edge_times[i] += edge_used[i];
            edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
        }
    }
    return;
}


void contest_calculate_95_with_maxrequire_sort4(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    
    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

    int base = 100;

    for(int i=0; i < data_dm_rowstore.size(); i++) {
        std::vector<int>& row = data_dm_rowstore[i];
        int sumrow = 0;
        for(auto num: row) sumrow += num;
        sort_time[i] = {sumrow, i};
    }

    std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

    for (size_t st = 0; st < sort_time.size(); st++) {
        int time = sort_time[st].second;
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        std::vector<int> edge_record_acm(edges_names.size(), 0);  // edge当前轮次累计
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            // 对单个时间内已经分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time || edge_used[edge_idx] != 1) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 对单个时间内未分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 经过以上所有edge节点已经分配超过5%次数

            // 先分配带宽剩余最多的，还是先分配之前单轮次分配带宽最多的？
            // 单轮次分配带宽最大记录排序
            std::vector<std::pair<int, int>> max_record_sort;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                max_record_sort.push_back(
                    {edge_max_record[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(max_record_sort.begin(), max_record_sort.end(),
                      std::greater<std::pair<int, int>>());

            // 根据评分分配
            while(cli_dm) {
                for (size_t i = 0; i < max_record_sort.size() && cli_dm; i++) {
                    int edge_idx = max_record_sort[i].second;
                    double score = double(edge_dist_num[edge_idx]) / max_record_sort.size();
                    int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                    int dist_v_in = score * base;
                    dist_v_in = dist_v_in * dist_v_in;
                    dist_v_in = std::min(dist_v_in, edge_rest_sb);
                    dist_v_in = std::min(dist_v_in, cli_dm);
                    cli_dm -= dist_v_in;
                    edge_rest_sb -= dist_v_in;
                    res[time][cli_idx][edge_idx] += dist_v_in;
                    edge_record_acm[edge_idx] += dist_v_in;
                }
            }
            
            if (cli_dm != 0) std::cout << cli_dm << std::endl;
        }

        for (size_t i = 0; i < edge_times.size(); i++) {
            edge_times[i] += edge_used[i];
            edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
        }
    }
    return;
}

void contest_calculate_95_with_maxrequire_sort5(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    int split_max = 1000;

    res.resize(data_dm_rowstore.size());  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0});

    int base = 100;

    for(int i=0; i < data_dm_rowstore.size(); i++) {
        std::vector<int>& row = data_dm_rowstore[i];
        int sumrow = 0;
        for(auto num: row) sumrow += num;
        sort_time[i] = {sumrow, i};
    }

    std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

    for (size_t st = 0; st < sort_time.size(); st++) {
        int time = sort_time[st].second;
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        res[time].resize(row.size());
        auto sb_map_cp = sb_map;
        std::vector<int> edge_used(edges_names.size(), 0);
        std::vector<int> edge_record_acm(edges_names.size(), 0);  // edge当前轮次累计
        for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            res[time][cli_idx].resize(edges_names.size(), 0);
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            // 对单个时间内已经分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time || edge_used[edge_idx] != 1) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 对单个时间内未分配过的edge进行分配
            for (int i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_times[edge_idx] >= five_percent_time) continue;
                edge_used[edge_idx] = 1;
                int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }

            // 经过以上所有edge节点已经分配超过5%次数

            while (cli_dm) {
                for (size_t i = 0; i < qos_map[cli_idx].size() && cli_dm; i++) {
                    int edge_idx = qos_map[cli_idx][i];
                    int dist_v = split_max / edge_dist_num[edge_idx];
                    int& edge_rest_sb = sb_map_cp[edges_names[edge_idx]];
                    dist_v = std::min(dist_v, cli_dm);
                    dist_v = std::min(dist_v, edge_rest_sb);
                    cli_dm -= dist_v;
                    edge_rest_sb -= dist_v;
                    res[time][cli_idx][edge_idx] += dist_v;
                }
            }
        }

        for (size_t i = 0; i < edge_times.size(); i++) {
            edge_times[i] += edge_used[i];
            // edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
        }
    }
    return;
}

// 对所有边缘节点可分配总需求排序
void contest_calculate_95_with_maxrequire_sort6(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::vector<std::vector<int>> edge_dist_clients, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    int split_max = 1000;

    res.resize(data_dm_rowstore.size(), std::vector<std::vector<int>>(client_names.size(), std::vector<int>(edges_names.size(), 0)));  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    std::vector<std::vector<std::pair<int, int>>> edge_require_sort(edges_names.size(), std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));  // 对所有边缘节点可分配的客户端总需求排序
    
    std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0}); // 对总需求量排序
    auto sb_map_copy = sb_map;
    std::vector<std::unordered_map<std::string, int>> sb_map_alltime(data_dm_rowstore.size(), sb_map_copy);
    // std::vector<std::vector<int>> data_dm_rowstore_cp = data_dm_rowstore;
    // int base = 100;

    for(int i=0; i < data_dm_rowstore.size(); i++) {
        std::vector<int>& row = data_dm_rowstore[i];
        int sumrow = 0;
        for(auto num: row) sumrow += num;
        sort_time[i] = {sumrow, i};
    }

    std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

    // for(int i=0; i < edges_names.size(); i++) {
    //     std::vector<std::pair<int, int>>& sort_require = edge_require_sort[i];
    //     auto& clis = edge_dist_clients[i];
    //     for(int j=0; j < data_dm_rowstore.size(); i++) {
    //         std::vector<int>& row = data_dm_rowstore[i];
    //         int sumrow = 0;
    //         for(auto cli: clis) sumrow += row[cli];
    //         sort_require[j] = {sumrow, j};
    //     }
    //     std::sort(sort_require.begin(), sort_require.end(), std::greater<std::pair<int, int>>());
    // }

    std::vector<std::pair<int, int>> sort_edge_dist_num;
    for (size_t i = 0; i < edges_names.size(); i++) {
        sort_edge_dist_num.push_back({edge_dist_num[i], i});
    }
    std::sort(sort_edge_dist_num.begin(), sort_edge_dist_num.end()); // 从小到大排列
    
    for(int i=0; i < edges_names.size(); i++) {
        int edge_idx = sort_edge_dist_num[i].second;
        std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
        auto& clis = edge_dist_clients[edge_idx];
        for(int j=0; j < data_dm_rowstore.size(); j++) {
            std::vector<int>& row = data_dm_rowstore[j];
            int sumrow = 0;
            for(auto cli: clis) sumrow += row[cli];
            sort_require[j] = {sumrow, j};
        }
        std::sort(sort_require.begin(), sort_require.end(), std::greater<std::pair<int, int>>());
        
        int five_count = 0;
        int require_idx = 0;
        while(five_count <= five_percent_time && require_idx < sort_require.size()) {
            int time = sort_require[require_idx].second;
            auto& sb_map_ref = sb_map_alltime[time];
            std::vector<int>& row = data_dm_rowstore[time];
            int dist_v_acc = 0;
            std::vector<std::pair<int, int>> sort_cli_dm(clis.size());
            for(int j=0; j < clis.size(); j++) {
                // int cli_dm = row[clis[j]];
                // sort_cli_dm[j] = {cli_dm, clis[j]};
                int cli_edge_nums = qos_map[clis[j]].size();
                sort_cli_dm[j] = {cli_edge_nums, clis[j]};
            }
            std::sort(sort_cli_dm.begin(), sort_cli_dm.end());
            for(auto& cli : sort_cli_dm) {
                int cli_idx = cli.second;
            // for(auto cli_idx: clis) {
                int& cli_dm = row[cli_idx];
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                dist_v_acc += dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }
            if(dist_v_acc != 0) five_count++;
            require_idx++;
        }
    }
    // 以上5%分配完

    for (size_t st = 0; st < sort_time.size(); st++) {
        int time = sort_time[st].second;
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        // res[time].resize(row.size());
        auto& sb_map_ref = sb_map_alltime[time];
        std::vector<int> edge_used(edges_names.size(), 0);
        std::vector<int> edge_record_acm(edges_names.size(), 0);  // edge当前轮次累计
        std::vector<std::pair<int, int>> sort_cli_dm_in(client_names.size());
        for(int j=0; j < client_names.size(); j++) {
            // int cli_dm = row[clis[j]];
            // sort_cli_dm[j] = {cli_dm, clis[j]};
            int cli_edge_nums = qos_map[j].size();
            sort_cli_dm_in[j] = {cli_edge_nums, j};
        }
        std::sort(sort_cli_dm_in.begin(), sort_cli_dm_in.end());
        for (auto& cli : sort_cli_dm_in) {
            int cli_idx = cli.second;
        // for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            // std::vector<std::pair<int, int>> max_record_sort;
            // for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
            //     max_record_sort.push_back(
            //         {edge_max_record[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            // }
            // std::sort(max_record_sort.begin(), max_record_sort.end(),
            //           std::greater<std::pair<int, int>>());

            // 使用已经超分配过的edge进行分配
            for (size_t i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (!edge_exceed[edge_idx]) continue;
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
                edge_record_acm[edge_idx] += dist_v;
                edge_max_record[edge_idx] = std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
            }

            // 使用未超分配过的edge进行分配
            for (size_t i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_exceed[edge_idx]) continue;
                edge_exceed[edge_idx] = true;
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
                edge_record_acm[edge_idx] += dist_v;
                edge_max_record[edge_idx] = std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
            }
        }

        for (size_t i = 0; i < edge_times.size(); i++) {
            edge_times[i] += edge_used[i];
            // edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
        }
    }
    return;
}


void contest_calculate_95_with_maxrequire_sort7(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::vector<std::vector<int>> edge_dist_clients, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    int split_max = 1000;

    res.resize(data_dm_rowstore.size(), std::vector<std::vector<int>>(client_names.size(), std::vector<int>(edges_names.size(), 0)));  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    std::vector<std::vector<std::pair<int, int>>> edge_require_sort(edges_names.size(), std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));  // 对所有边缘节点可分配的客户端总需求排序
    
    std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0}); // 对总需求量排序
    auto sb_map_copy = sb_map;
    std::vector<std::unordered_map<std::string, int>> sb_map_alltime(data_dm_rowstore.size(), sb_map_copy);

    for(int i=0; i < data_dm_rowstore.size(); i++) {
        std::vector<int>& row = data_dm_rowstore[i];
        int sumrow = 0;
        for(auto num: row) sumrow += num;
        sort_time[i] = {sumrow, i};
    }

    std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

    std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
    for (size_t i = 0; i < edges_names.size(); i++) {
        sort_edge_avg_dist_highest_value.push_back({sb_map[edges_names[i]] / (edge_dist_num[i] + 0.1), i});
    }
    std::sort(sort_edge_avg_dist_highest_value.begin(), sort_edge_avg_dist_highest_value.end(), std::greater<std::pair<double, int>>());
    
    for(int i=0; i < edges_names.size(); i++) {
        int edge_idx = sort_edge_avg_dist_highest_value[i].second;
        std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
        auto& clis = edge_dist_clients[edge_idx];
        for(int j=0; j < data_dm_rowstore.size(); j++) {
            std::vector<int>& row = data_dm_rowstore[j];
            int sumrow = 0;
            for(auto cli: clis) sumrow += row[cli];
            sort_require[j] = {sumrow, j};
        }
        std::sort(sort_require.begin(), sort_require.end(), std::greater<std::pair<int, int>>());
        
        int five_count = 0;
        int require_idx = 0;
        while(five_count <= five_percent_time && require_idx < sort_require.size()) {
            int time = sort_require[require_idx].second;
            auto& sb_map_ref = sb_map_alltime[time];
            std::vector<int>& row = data_dm_rowstore[time];
            int dist_v_acc = 0;
            for(auto cli_idx: clis) {
                int& cli_dm = row[cli_idx];
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                dist_v_acc += dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }
            if(dist_v_acc != 0) five_count++;
            require_idx++;
        }
    }
    // 以上5%分配完

    for (size_t st = 0; st < sort_time.size(); st++) {
        int time = sort_time[st].second;
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        auto& sb_map_ref = sb_map_alltime[time];
        std::vector<int> edge_used(edges_names.size(), 0);
        std::vector<int> edge_record_acm(edges_names.size(), 0);  // edge当前轮次累计

        std::vector<std::pair<int, int>> sort_clis(client_names.size());
        for(int cli_idx=0; cli_idx < client_names.size(); cli_idx++) {
            // int cli_dm = row[cli_idx];
            // sort_clis[cli_idx] = {cli_dm, cli_idx};
            int cli_edge_nums = qos_map[cli_idx].size();
            sort_clis[cli_idx] = {cli_edge_nums, cli_idx};
        }
        std::sort(sort_clis.begin(), sort_clis.end(), std::greater<std::pair<int, int>>());

        
        for (size_t i = 0; i < sort_clis.size(); i++) {
            int cli_idx = sort_clis[i].second;
        // for (int cli_idx = 0; cli_idx < client_names.size(); cli_idx++) {
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            std::vector<std::pair<int, int>> max_record_sort;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                max_record_sort.push_back(
                    {edge_max_record[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(max_record_sort.begin(), max_record_sort.end(),
                      std::greater<std::pair<int, int>>());

            // 使用已经超分配过的edge进行分配
            for (size_t i = 0; i < max_record_sort.size() && cli_dm; i++) {
                int edge_idx = max_record_sort[i].second;
                if (!edge_exceed[edge_idx]) continue;
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
                edge_record_acm[edge_idx] += dist_v;
                edge_max_record[edge_idx] = std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
            }

            // 使用未超分配过的edge进行分配
            for (size_t i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_exceed[edge_idx]) continue;
                edge_exceed[edge_idx] = true;
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
                edge_record_acm[edge_idx] += dist_v;
                edge_max_record[edge_idx] = std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
            }
        }

        for (size_t i = 0; i < edge_times.size(); i++) {
            edge_times[i] += edge_used[i];
            // edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
        }
    }
    return;
}


void contest_calculate_95_with_maxrequire_sort8(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::vector<std::vector<int>> edge_dist_clients, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    int split_max = 1000;

    res.resize(data_dm_rowstore.size(), std::vector<std::vector<int>>(client_names.size(), std::vector<int>(edges_names.size(), 0)));  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    std::vector<std::vector<std::pair<int, int>>> edge_require_sort(edges_names.size(), std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));  // 对所有边缘节点可分配的客户端总需求排序
    
    std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0}); // 对总需求量排序
    auto sb_map_copy = sb_map;
    std::vector<std::unordered_map<std::string, int>> sb_map_alltime(data_dm_rowstore.size(), sb_map_copy);

    for(int i=0; i < data_dm_rowstore.size(); i++) {
        std::vector<int>& row = data_dm_rowstore[i];
        int sumrow = 0;
        for(auto num: row) sumrow += num;
        sort_time[i] = {sumrow, i};
    }

    // std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

    // std::vector<std::pair<double, int>> sort_edge_avg_dist_highest_value;
    // for (size_t i = 0; i < edges_names.size(); i++) {
    //     sort_edge_avg_dist_highest_value.push_back({sb_map[edges_names[i]] / (edge_dist_num[i] + 0.1), i});
    // }
    // std::sort(sort_edge_avg_dist_highest_value.begin(), sort_edge_avg_dist_highest_value.end(), std::greater<std::pair<double, int>>());
    std::vector<std::pair<int, int>> sort_edge_dist_num;
    for (size_t i = 0; i < edges_names.size(); i++) {
        sort_edge_dist_num.push_back({edge_dist_num[i], i});
    }
    std::sort(sort_edge_dist_num.begin(), sort_edge_dist_num.end()); // 从小到大排列

    for(int i=0; i < edges_names.size(); i++) {
        int edge_idx = sort_edge_dist_num[i].second;
        std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
        auto& clis = edge_dist_clients[edge_idx];
        for(int j=0; j < data_dm_rowstore.size(); j++) {
            std::vector<int>& row = data_dm_rowstore[j];
            int sumrow = 0;
            for(auto cli: clis) sumrow += row[cli];
            sort_require[j] = {sumrow, j};
        }
        std::sort(sort_require.begin(), sort_require.end(), std::greater<std::pair<int, int>>());
        
        std::vector<std::pair<int, int>> sort_cli_dm(clis.size());
        for(int j=0; j < clis.size(); j++) {
            // int cli_dm = row[clis[j]];
            // sort_cli_dm[j] = {cli_dm, clis[j]};
            int cli_edge_nums = qos_map[clis[j]].size();
            sort_cli_dm[j] = {cli_edge_nums, clis[j]};
        }
        std::sort(sort_cli_dm.begin(), sort_cli_dm.end());

        int five_count = 0;
        int require_idx = 0;
        while(five_count <= five_percent_time && require_idx < sort_require.size()) {
            int time = sort_require[require_idx].second;
            auto& sb_map_ref = sb_map_alltime[time];
            std::vector<int>& row = data_dm_rowstore[time];
            int dist_v_acc = 0;
            for(auto cli_idx: clis) {
            // for(auto& cli:sort_cli_dm) {
            //     int cli_idx = cli.second;
                int& cli_dm = row[cli_idx];
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                dist_v_acc += dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }
            if(dist_v_acc != 0) five_count++;
            require_idx++;
        }
    }

    // 以上5%分配完
    for(int i=0; i < edges_names.size(); i++) {
        int edge_idx = i;
        std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
        auto& clis = edge_dist_clients[edge_idx];
        for(int j=0; j < data_dm_rowstore.size(); j++) {
            std::vector<int>& row = data_dm_rowstore[j];
            int sumrow = 0;
            for(auto cli: clis) sumrow += row[cli];
            sort_require[j] = {sumrow, j};
        }
        std::sort(sort_require.begin(), sort_require.end(), std::greater<std::pair<int, int>>());
    }

    std::vector<std::pair<int, int>> sort_edge_score_value;
    for (size_t i = 0; i < edges_names.size(); i++) {
        // sort_edge_score_value.push_back({sb_map[edges_names[i]] * edge_dist_num[i], i});
        sort_edge_score_value.push_back({edge_dist_num[i], i});
        // sort_edge_score_value.push_back({sb_map[edges_names[i]], i});
        // sort_edge_score_value.push_back({edge_require_sort[i][0].first * edge_dist_num[i], i});
    }
    std::sort(sort_edge_score_value.begin(), sort_edge_score_value.end(), std::greater<std::pair<int, int>>());
    
    for(int i=0; i < edges_names.size(); i++) {
        int edge_idx = sort_edge_score_value[i].second;
        std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
        auto& clis = edge_dist_clients[edge_idx];
        // for(int j=0; j < data_dm_rowstore.size(); j++) {
        //     std::vector<int>& row = data_dm_rowstore[j];
        //     int sumrow = 0;
        //     for(auto cli: clis) sumrow += row[cli];
        //     sort_require[j] = {sumrow, j};
        // }
        // std::sort(sort_require.begin(), sort_require.end(), std::greater<std::pair<int, int>>());
        
        std::vector<std::pair<int, int>> sort_cli_dm_in(clis.size());
        for(int j=0; j < clis.size(); j++) {
            // int cli_dm = row[clis[j]];
            // sort_cli_dm[j] = {cli_dm, clis[j]};
            int cli_edge_nums = qos_map[clis[j]].size();
            sort_cli_dm_in[j] = {cli_edge_nums, clis[j]};
        }
        std::sort(sort_cli_dm_in.begin(), sort_cli_dm_in.end(), std::greater<std::pair<int, int>>());

        for(int require_idx=0; require_idx < sort_require.size(); require_idx++) {
            if(sort_require[require_idx].first == 0) break;
            int time = sort_require[require_idx].second;
            auto& sb_map_ref = sb_map_alltime[time];

            std::vector<int>& row = data_dm_rowstore[time];
            

            int dist_v_acc = 0;
            for(auto cli: sort_cli_dm_in) {
                int cli_idx = cli.second;
                int& cli_dm = row[cli_idx];
                if(cli_dm == 0) continue;
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                dist_v_acc += dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }
            
        }
    }


    return;
}


void contest_calculate_95_with_maxrequire_sort9(
    std::vector<std::string>& client_names, std::vector<std::string>& edges_names,
    std::vector<std::vector<int>>& data_dm_rowstore, std::vector<std::vector<int>>& qos_map,
    std::vector<int>& edge_dist_num, std::vector<std::vector<int>> edge_dist_clients, std::unordered_map<std::string, int>& sb_map,
    int qos_constrain, std::vector<std::vector<std::vector<int>>>& res) {
    int split_max = 1000;

    res.resize(data_dm_rowstore.size(), std::vector<std::vector<int>>(client_names.size(), std::vector<int>(edges_names.size(), 0)));  // 时间 客户节点序号 edge序号 分配数量
    std::vector<int> edge_times(edges_names.size(), 0);
    int five_percent_time = data_dm_rowstore.size() * 0.05 - 1;
    std::vector<bool> edge_exceed(edges_names.size(), false);
    std::vector<int> edge_max_record(edges_names.size(), 0);  // edge节点超分配的最大值
    std::vector<std::vector<std::pair<int, int>>> edge_require_sort(edges_names.size(), std::vector<std::pair<int, int>>(data_dm_rowstore.size(), {0, 0}));  // 对所有边缘节点可分配的客户端总需求排序
    
    std::vector<std::pair<int, int>> sort_time(data_dm_rowstore.size(), {0, 0}); // 对总需求量排序
    auto sb_map_copy = sb_map;
    std::vector<std::unordered_map<std::string, int>> sb_map_alltime(data_dm_rowstore.size(), sb_map_copy);
    // std::vector<std::vector<int>> data_dm_rowstore_cp = data_dm_rowstore;
    // int base = 100;

    for(int i=0; i < data_dm_rowstore.size(); i++) {
        std::vector<int>& row = data_dm_rowstore[i];
        int sumrow = 0;
        for(auto num: row) sumrow += num;
        sort_time[i] = {sumrow, i};
    }

    std::sort(sort_time.begin(), sort_time.end(), std::greater<std::pair<int, int>>());

    // for(int i=0; i < edges_names.size(); i++) {
    //     std::vector<std::pair<int, int>>& sort_require = edge_require_sort[i];
    //     auto& clis = edge_dist_clients[i];
    //     for(int j=0; j < data_dm_rowstore.size(); i++) {
    //         std::vector<int>& row = data_dm_rowstore[i];
    //         int sumrow = 0;
    //         for(auto cli: clis) sumrow += row[cli];
    //         sort_require[j] = {sumrow, j};
    //     }
    //     std::sort(sort_require.begin(), sort_require.end(), std::greater<std::pair<int, int>>());
    // }

    std::vector<std::pair<int, int>> sort_edge_dist_num;
    for (size_t i = 0; i < edges_names.size(); i++) {
        sort_edge_dist_num.push_back({edge_dist_num[i], i});
    }
    std::sort(sort_edge_dist_num.begin(), sort_edge_dist_num.end()); // 从小到大排列
    
    for(int i=0; i < edges_names.size(); i++) {
        int edge_idx = sort_edge_dist_num[i].second;
        std::vector<std::pair<int, int>>& sort_require = edge_require_sort[edge_idx];
        auto& clis = edge_dist_clients[edge_idx];
        for(int j=0; j < data_dm_rowstore.size(); j++) {
            std::vector<int>& row = data_dm_rowstore[j];
            int sumrow = 0;
            for(auto cli: clis) sumrow += row[cli];
            sort_require[j] = {sumrow, j};
        }
        std::sort(sort_require.begin(), sort_require.end(), std::greater<std::pair<int, int>>());
        
        int five_count = 0;
        int require_idx = 0;
        while(five_count <= five_percent_time && require_idx < sort_require.size()) {
            int time = sort_require[require_idx].second;
            auto& sb_map_ref = sb_map_alltime[time];
            std::vector<int>& row = data_dm_rowstore[time];
            int dist_v_acc = 0;
            std::vector<std::pair<int, int>> sort_cli_dm(clis.size());
            for(int j=0; j < clis.size(); j++) {
                // int cli_dm = row[clis[j]];
                // sort_cli_dm[j] = {cli_dm, clis[j]};
                int cli_edge_nums = qos_map[clis[j]].size();
                sort_cli_dm[j] = {cli_edge_nums, clis[j]};
            }
            std::sort(sort_cli_dm.begin(), sort_cli_dm.end());
            for(auto& cli : sort_cli_dm) {
                int cli_idx = cli.second;
            // for(auto cli_idx: clis) {
                int& cli_dm = row[cli_idx];
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                dist_v_acc += dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
            }
            if(dist_v_acc != 0) five_count++;
            require_idx++;
        }
    }
    // 以上5%分配完

    for (size_t st = 0; st < sort_time.size(); st++) {
        int time = sort_time[st].second;
        std::vector<int> row = data_dm_rowstore[time];  // 每个时间段客户端需求的带宽
        // res[time].resize(row.size());
        auto& sb_map_ref = sb_map_alltime[time];
        std::vector<int> edge_used(edges_names.size(), 0);
        std::vector<int> edge_record_acm(edges_names.size(), 0);  // edge当前轮次累计
        std::vector<std::pair<int, int>> sort_cli_dm_in(client_names.size());
        for(int j=0; j < client_names.size(); j++) {
            // int cli_dm = row[clis[j]];
            // sort_cli_dm[j] = {cli_dm, clis[j]};
            int cli_edge_nums = qos_map[j].size();
            sort_cli_dm_in[j] = {cli_edge_nums, j};
        }
        std::sort(sort_cli_dm_in.begin(), sort_cli_dm_in.end());
        for (auto& cli : sort_cli_dm_in) {
            int cli_idx = cli.second;
        // for (size_t cli_idx = 0; cli_idx < row.size(); cli_idx++) {
            int cli_dm = row[cli_idx];

            std::vector<std::pair<int, int>> sort_array;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                sort_array.push_back({edge_dist_num[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(sort_array.begin(), sort_array.end(), std::greater<std::pair<int, int>>());

            std::vector<std::pair<int, int>> max_record_sort;
            for (size_t i = 0; i < qos_map[cli_idx].size(); i++) {
                max_record_sort.push_back(
                    {edge_max_record[qos_map[cli_idx][i]], qos_map[cli_idx][i]});
            }
            std::sort(max_record_sort.begin(), max_record_sort.end(),
                      std::greater<std::pair<int, int>>());

            // 使用已经超分配过的edge进行分配
            for (size_t i = 0; i < max_record_sort.size() && cli_dm; i++) {
                int edge_idx = max_record_sort[i].second;
                if (!edge_exceed[edge_idx]) continue;
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
                edge_record_acm[edge_idx] += dist_v;
                edge_max_record[edge_idx] = std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
            }

            // 使用未超分配过的edge进行分配
            for (size_t i = 0; i < sort_array.size() && cli_dm; i++) {
                int edge_idx = sort_array[i].second;
                if (edge_exceed[edge_idx]) continue;
                edge_exceed[edge_idx] = true;
                int& edge_rest_sb = sb_map_ref[edges_names[edge_idx]];
                int dist_v = edge_rest_sb;
                dist_v = std::min(dist_v, cli_dm);
                cli_dm -= dist_v;
                edge_rest_sb -= dist_v;
                res[time][cli_idx][edge_idx] += dist_v;
                edge_record_acm[edge_idx] += dist_v;
                edge_max_record[edge_idx] = std::max(edge_max_record[edge_idx], edge_record_acm[edge_idx]);
            }
        }

        for (size_t i = 0; i < edge_times.size(); i++) {
            edge_times[i] += edge_used[i];
            // edge_max_record[i] = std::max(edge_max_record[i], edge_record_acm[i]);
        }
    }
    return;
}


void handle_output(std::vector<std::vector<std::vector<int>>>& res,
                   std::vector<std::string>& client_names, std::vector<std::string>& edges_names) {
#ifdef DEBUG
    std::ofstream fout("./output/solution.txt", std::ios_base::trunc);
#else
    std::ofstream fout("/output/solution.txt", std::ios_base::trunc);
#endif
    for (size_t time = 0; time < res.size(); time++) {
        auto& output_time = res[time];
        for (size_t cli_idx = 0; cli_idx < output_time.size(); cli_idx++) {
            fout << client_names[cli_idx] << ":";
            // int sum=0;
            bool is_first = true;
            for (size_t edge_idx = 0; edge_idx < output_time[cli_idx].size(); edge_idx++) {
                if (output_time[cli_idx][edge_idx] != 0) {
                    if (!is_first)
                        fout << ",";
                    else
                        is_first = false;
                    fout << "<" << edges_names[edge_idx] << "," << output_time[cli_idx][edge_idx]
                         << ">";
                }
            }
            fout << std::endl;
            // fout << " sum: " << sum;
            // fout << std::endl;
        }
    }
    return;
}

int main() {
    std::vector<std::string> client_names;  // 客户节点名字
    std::vector<std::string> edges_names;   // 边缘节点名字
    std::vector<std::vector<int>>
        data_dm_rowstore;  // demand行存，外idx为时间排序，内idx根据客户节点vector排序
    std::vector<std::vector<int>>
        qos_map;  // 根据qos_constrain过滤后客户节点可用的edges，vector序号对应着client_names里面的顺序，edges序号对应着edges_names里面的序号
    std::vector<int> edge_dist_num;               // 每个edge可以分发的节点数
    std::vector<std::vector<int>> edge_dist_clients;  // 每个edge可以分发的客户节点
    std::unordered_map<std::string, int> sb_map;  // 带宽映射，根据edge name获得带宽
    int qos_constrain;

    handle_contest_input(client_names, edges_names, data_dm_rowstore, qos_map, edge_dist_num,
                         edge_dist_clients, sb_map, qos_constrain);
    // input test
    // std::cout << data_dm_rowstore[99][1] << std::endl;
    // std::cout << sb_map["Dm"] << std::endl;
    // std::cout << qos_constrain << std::endl;

    std::vector<std::vector<std::vector<int>>> res_for_print;
    // contest_calculate_95_with_rank(client_names, edges_names, data_dm_rowstore, qos_map,
    //                                     edge_dist_num, sb_map, qos_constrain, res_for_print);

    contest_calculate_95_with_maxrequire_sort6(client_names, edges_names, data_dm_rowstore, qos_map,
                                        edge_dist_num, edge_dist_clients, sb_map, qos_constrain, res_for_print);
    handle_output(res_for_print, client_names, edges_names);
    return 0;
}
