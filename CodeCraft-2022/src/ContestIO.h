#pragma once
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <unordered_set>

class ContestIO {
    public:
    void handle_config() {
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
            std::istringstream cfg_line_iss1(cfg_line);
            std::string elem;
            getline(cfg_line_iss1, elem, '=');
            getline(cfg_line_iss1, elem, '=');  // 获取qos_constrain;
            qos_constrain = atoi(elem.c_str());

            getline(fp_cfg, cfg_line);  // 获取第三行
            if (cfg_line.size() && cfg_line[cfg_line.size() - 1] == '\r') {
                cfg_line = cfg_line.substr(0, cfg_line.size() - 1);
            }

            std::istringstream cfg_line_iss2(cfg_line);
            getline(cfg_line_iss2, elem, '=');
            getline(cfg_line_iss2, elem, '=');  // 获取qos_constrain;
            base_cost = atoi(elem.c_str());
    }

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
               std::vector<std::vector<std::vector<std::pair<int, std::string>>>>& data_dm_rowstore) {
        size_t len = data_dm["mtime"].size();
        int i = 0;
        int row_idx = 0;
        while (i < len) {
            std::string time = data_dm["mtime"][i];
            std::vector<std::vector<std::pair<int, std::string>>> row(cols_client.size());
            data_dm_rowstore.emplace_back(std::move(row));
            while(i < len) {
                if(data_dm["mtime"][i] != time) break;
                std::string stream_id = data_dm["stream_id"][i];
                for (size_t j = 0; j < cols_client.size(); j++) {
                    data_dm_rowstore[row_idx][j].push_back({atoi(data_dm[cols_client[j]][i].c_str()), stream_id});
                }
                i++;
            }
            row_idx++;
        }
        // for(int i=0; i<data_dm_rowstore[0][0].size(); i++) {
        //     std::cout << data_dm_rowstore[0][0][i].first << " " << data_dm_rowstore[0][0][i].second << std::endl;
        // }
    }

    void handle_qos_filter(std::unordered_map<std::string, std::vector<std::string>>& data_qos,
                        std::vector<std::string>& cols_client, std::vector<std::vector<int>>& output,
                        std::vector<int>& edge_dist_num,
                        std::vector<std::vector<int>>& edge_dist_clients,
                        std::vector<std::string>& cols_edges, int qos_constrain) {
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

    void handle_contest_input() {
        handle_config();

    #ifdef DEBUG
        std::ifstream fp_dm("./data/demand.csv");  //定义声明一个ifstream对象，指定文件路径
    #else
        std::ifstream fp_dm("/data/demand.csv");  //定义声明一个ifstream对象，指定文件路径
    #endif
        std::unordered_map<std::string, std::vector<std::string>> data_dm;
        handle_csv(fp_dm, data_dm, client_names);
        client_names.erase(client_names.begin());  // 删除首位mtime
        client_names.erase(client_names.begin());  // 删除第二位stream_id
        handle_dm(data_dm, client_names, data_dm_rowstore);
        fp_dm.close();

    #ifdef DEBUG
        std::ifstream fp_qos("./data/qos.csv");
    #else
        std::ifstream fp_qos("/data/qos.csv");
    #endif
        std::unordered_map<std::string, std::vector<std::string>> data_qos;
        std::vector<std::string> cols_qos;
        qos_map.resize(client_names.size());
        handle_csv(fp_qos, data_qos, cols_qos);
        handle_qos_filter(data_qos, client_names, qos_map, edge_dist_num, edge_dist_clients, edges_names,
                        qos_constrain);
        fp_qos.close();
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
        fp_sb.close();
    }

    void handle_output(std::vector<std::vector<std::vector<std::vector<std::pair<int, std::string>>>>>& res) {
    #ifdef DEBUG
        std::ofstream fout("./output/solution.txt", std::ios_base::trunc);
    #else
        std::ofstream fout("/output/solution.txt", std::ios_base::trunc);
    #endif
        for (size_t time = 0; time < res.size(); time++) {
            auto& output_time = res[time];
            for (size_t cli_idx = 0; cli_idx < output_time.size(); cli_idx++) {
                fout << client_names[cli_idx] << ":";
                bool is_first = true;
                for (size_t edge_idx = 0; edge_idx < output_time[cli_idx].size(); edge_idx++) {
                    if (output_time[cli_idx][edge_idx].size() > 0) {
                        if(!is_first) fout << ",";
                        else is_first = false;
                        fout << "<" << edges_names[edge_idx];
                        for(int i = 0; i < output_time[cli_idx][edge_idx].size(); i++) {
                            fout << ",";
                            fout << output_time[cli_idx][edge_idx][i].second;
                        }
                        fout << ">";
                    }
                }
                fout << std::endl;
            }
        }
        fout.close();
        return;
    }

    public:
        int qos_constrain;
        int base_cost;
        std::vector<std::string> client_names;  // 客户节点名字
        std::vector<std::string> edges_names;   // 边缘节点名字
        std::vector<std::vector<std::vector<std::pair<int, std::string>>>>
            data_dm_rowstore;  // demand行存，外idx为时间排序，中idx根据客户节点vector排序, 内idx为需求量与流名称的pair的idx（这个没有顺序）
        std::vector<std::vector<int>>
            qos_map;  // 根据qos_constrain过滤后客户节点可用的edges，vector序号对应着client_names里面的顺序，edges序号对应着edges_names里面的序号
        std::vector<int> edge_dist_num;                   // 每个edge可以分发的节点数
        std::vector<std::vector<int>> edge_dist_clients;  // 每个edge可以分发的客户节点
        std::unordered_map<std::string, int> sb_map;      // 带宽映射，根据edge name获得带宽        
};
