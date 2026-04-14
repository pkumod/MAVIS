#include "super_query_graph.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "pretty_print.h"

void TreePartitionGraph::PrintSQNs() {
    /* output each super query node */
    for (unsigned i = 0; i < sqns_.size(); ++i) {
        // output node inside super query node
        std::cout << "super query node " << i << ": ";
        std::cout << sqns_[i].node_list_ << std::endl;
        std::cout << "root_var_id_: " << sqns_[i].root_ids_[0] << std::endl;
        // output is tree or not
        std::cout << std::endl;
    }
}

void TreePartitionGraph::PrintSQEdges() {
    // output each super edge
    for (unsigned i = 0; i < sqns_.size(); ++i) {
        for (unsigned j = 0; j < adj_[i].size(); ++j) {
            auto u = i;
            auto v = adj_[i][j].dst;
            std::cout << "super edge " << u << " -> " << v << ": \n";
            std::cout << "  source: " << adj_[i][j].src_set << "\n";
            std::cout << "  destination: " << adj_[i][j].dst_set << "\n";
            std::cout << "  src dst pairs: " << adj_[i][j].src_dst_pairs << "\n";
            std::cout << std::endl;
        }
    }
}

void TreePartitionGraph::Print() {
    PRINT_YELLOW("+======== SuperQueryGraph ========+\n");
    std::cout << "total super query node num: " << sqns_.size() << std::endl;
    PrintSQNs();
    PrintSQEdges();
    PRINT_YELLOW("-======== SuperQueryGraph ========-\n");
}


void TreePartitionGraph::PrintDot(std::ofstream& fout) {
    std::vector<std::string> node_colors;
    std::vector<std::vector<std::string>> edge_colors;

    for (unsigned i = 0; i < sqns_.size(); ++i) {
        node_colors.push_back(GetRandomColors());
    }

    edge_colors.resize(sqns_.size());
    for (unsigned i = 0; i < sqns_.size(); ++i) {
        edge_colors[i].resize(sqns_.size());
    }
    for (unsigned i = 0; i < sqns_.size(); ++i) {
        edge_colors[i][i] = "black";
        for (unsigned j = i + 1; j < sqns_.size(); ++j) {
            edge_colors[i][j] = edge_colors[j][i] = GetRandomColors();
        }
    }

    fout << "graph G {\n";
    for (unsigned i = 0; i < sqns_.size(); ++i) {
        fout << "subgraph cluster_" << i << " {\n";
        fout << "style=filled;\n";
        fout << "label=\"sqn_" << i << "\";\n";
        for (auto node_id : sqns_[i].node_list_) {
            auto node_name = node_id;
            auto node_label = query_graph_->getVertexLabel(node_id);
            fout << "  n" << node_name << "_" << node_label << " [color=\"" << node_colors[i] << "\"];\n";
        }
        fout << "}\n";
    }
    for (unsigned s = 0; s < query_graph_->getVerticesCount(); ++s) {
        unsigned len;
        const unsigned* os = query_graph_->getVertexNeighbors(s, len);
        for (int i = 0; i < len; ++i) {
            unsigned o = os[i];
            if (s > o) continue;
            auto s_label = query_graph_->getVertexLabel(s);
            auto o_label = query_graph_->getVertexLabel(o);
            auto s_super_node = GetId2SQNId(s);
            auto o_super_node = GetId2SQNId(o);
            auto edge_color = edge_colors[s_super_node][o_super_node];
            fout << "  n" << s << "_" << s_label << " -- n" << o << "_" << o_label << "[color=\"" << edge_color << "\"];\n";
        }
    }
    fout << "}\n";
}

void TreePartitionGraph::PrintDot(std::string file_name) {
    std::ofstream fout(file_name);
    if (!fout.is_open()) {
        std::cerr << "Error: cannot open file " << file_name << std::endl;
        return;
    }
    PrintDot(fout);
    fout.close();
}

void TreePartitionGraph::Build(const std::vector<std::vector<ui>>& super_nodes) {
    for (unsigned i = 0; i < super_nodes.size(); ++i) {
        sqns_.push_back(SuperQueryNode(i, super_nodes[i], query_graph_.get()));
    }

    // build the connecting structure among super query nodes
    adj_.resize(sqns_.size());
    adj_mat_.resize(sqns_.size());
    for (unsigned i = 0; i < sqns_.size(); ++i) {
        adj_mat_[i].resize(sqns_.size());
    }
    for (ui u = 0; u < sqns_.size(); ++u) {
        for (ui u1 = 0; u1 < sqns_.size(); ++u1) {
            if (u == u1) continue;

            std::vector<ui> src_set, dst_set;
            std::vector<PUU> src_dst_pairs;
            for (auto v: sqns_[u].node_list_) {
                for (auto v1: sqns_[u1].node_list_) {
                    if (query_graph_->checkEdgeExistence(v, v1)) {
                        src_set.push_back(v);
                        dst_set.push_back(v1);
                        src_dst_pairs.push_back(PUU(v, v1));
                    }
                }
            }
            if (src_set.empty()) continue;

            adj_[u].emplace_back();
            adj_[u].back().dst = u1;
            adj_[u].back().src_set = src_set;
            adj_[u].back().dst_set = dst_set;
            adj_[u].back().src_dst_pairs = src_dst_pairs;
            std::sort(adj_[u].back().src_set.begin(), adj_[u].back().src_set.end());
            std::sort(adj_[u].back().dst_set.begin(), adj_[u].back().dst_set.end());
            adj_[u].back().src_set.erase(std::unique(adj_[u].back().src_set.begin(), adj_[u].back().src_set.end()), adj_[u].back().src_set.end());
            adj_[u].back().dst_set.erase(std::unique(adj_[u].back().dst_set.begin(), adj_[u].back().dst_set.end()), adj_[u].back().dst_set.end());
            adj_mat_[u][u1] = adj_[u].back();
            // std::cout << "u: " << u << ", u1: " << u1 << std::endl;
            // std::cout << "src_set: " << adj_[u].back().src_set << std::endl;
            // std::cout << "dst_set: " << adj_[u].back().dst_set << std::endl;
            // std::cout << "src_dst_pairs: " << adj_[u].back().src_dst_pairs << std::endl;
        }
    }
}

void TreePartitionGraph::Init() {
    query_graph_ = nullptr;
    sqns_.clear();
    adj_.clear();
    adj_mat_.clear();
}

void TreePartitionGraph::Dump(std::string file_name) {
    query_graph_->Dump(file_name + ".graph");
    // dump partition
    std::ofstream fout(file_name + ".partition");
    fout << sqns_.size() << std::endl;
    for (unsigned i = 0; i < sqns_.size(); ++i) {
        fout << sqns_[i].node_list_.size() << " ";
        for (auto node_id : sqns_[i].node_list_) {
            fout << node_id << " ";
        }
        fout << std::endl;
    }
    fout.close();
}

void TreePartitionGraph::Dump(std::ofstream& fout) {
    query_graph_->Dump(fout);
    // dump partition
    fout << sqns_.size() << std::endl;
    for (unsigned i = 0; i < sqns_.size(); ++i) {
        fout << sqns_[i].node_list_.size() << " ";
        for (auto node_id : sqns_[i].node_list_) {
            fout << node_id << " ";
        }
        fout << std::endl;
    }
}

void TreePartitionGraph::Load(std::string file_name) {
    Init();
    {
        std::ifstream fin(file_name + ".graph");
        query_graph_ = std::make_unique<Graph>(true);
        query_graph_->loadGraphFromIStream(fin);
        fin.close();
    }
    {
        std::ifstream fin(file_name + ".partition");
        std::vector<std::vector<unsigned>> partition;
        int partition_num;
        fin >> partition_num;
        partition.resize(partition_num);
        for (int i = 0; i < partition_num; ++i) {
            int node_num;
            fin >> node_num;
            partition[i].resize(node_num);
            for (int j = 0; j < node_num; ++j) {
                fin >> partition[i][j];
            }
        }
        Build(partition);
        fin.close();
    }
}

void TreePartitionGraph::Load(std::ifstream& fin) {
    Init();
    // load query graph
    query_graph_ = std::make_unique<Graph>(true);
    query_graph_->loadGraphFromIStream(fin);
    // load partition to rebuild super query graph
    std::vector<std::vector<unsigned>> partition;
    int partition_num;
    fin >> partition_num;
    partition.resize(partition_num);
    for (int i = 0; i < partition_num; ++i) {
        int node_num;
        fin >> node_num;
        partition[i].resize(node_num);
        for (int j = 0; j < node_num; ++j) {
            fin >> partition[i][j];
        }
    }
    Build(partition);
}

void TreePartitionGraph::Copy(const TreePartitionGraph& other) {
    query_graph_.reset(other.query_graph_->Clone());
    sqns_ = other.sqns_;
    adj_ = other.adj_;
    adj_mat_ = other.adj_mat_;
}

void TreeDecompositionGraph::Build(const std::vector<std::vector<ui>>& super_nodes, const std::vector<std::vector<ui>>& links) {
    adj_.resize(sqns_.size());
    adj_mat_.resize(sqns_.size());  
    for (ui i = 0; i < sqns_.size(); ++i) adj_mat_[i].resize(sqns_.size());

    for (ui u = 0; u < sqns_.size(); ++u) {
        for (auto u1: links[u]) {
            SuperEdgeInfo edge_info;
            edge_info.dst = u1;
            for (auto v: sqns_[u].node_list_) {
                for (auto v1: sqns_[u1].node_list_) {
                    if (v == v1) {
                        edge_info.src_set.push_back(v);
                        edge_info.dst_set.push_back(v1);
                        edge_info.src_dst_pairs.emplace_back(v, v1);
                        break;
                    }
                }
            }
            adj_[u].push_back(edge_info);
            adj_mat_[u][u1] = edge_info;
        }
    }

    for (unsigned i = 0; i < super_nodes.size(); ++i) {
        sqns_.push_back(SuperQueryNode(i, super_nodes[i], query_graph_.get()));
    }
}


void TreeDecompositionGraph::PrintDot(std::string file_name) {
    std::ofstream fout(file_name);
    fout << "graph G {\n";
    for (unsigned i = 0; i < sqns_.size(); ++i) {
        fout << "subgraph cluster_" << i << " {\n";
        fout << "style=filled;\n";
        fout << "label=\"sqn_" << i << "\";\n";
        for (auto u : sqns_[i].node_list_) {
            fout << "  n" << i << "_" << u << ";\n";
        }
        for (auto u: sqns_[i].node_list_) {
            for (auto u1: sqns_[i].link_info_[u]) {
                if (u < u1) {
                    fout << "  n" << i << "_" << u << " -- n" << i << "_" << u1 << ";\n";
                }
            }
        }
        fout << "}\n";
    }
    for (ui u = 0; u < sqns_.size(); ++u) {
        for (ui u1 = u + 1; u1 < sqns_.size(); ++u1) {
            if (adj_mat_[u][u1].src_set.size()) {
                for (auto v: sqns_[u].node_list_) {
                    for (auto v1: sqns_[u1].node_list_) {
                        if (query_graph_->checkEdgeExistence(v, v1)) {
                            fout << "  n" << u << "_" << v << " -- n" << u1 << "_" << v1 << " [color=\"red\"];\n";
                        }
                    }
                }
            }
        }
    }
    fout << "}\n";
    fout.close();
}

bool SuperQueryNode::HaveOuterLink(int u, Graph *query_graph) {
    ui len;
    const ui* neighbors = query_graph->getVertexNeighbors(u, len);
    for (int i = 0; i < len; ++i) {
        ui o = neighbors[i];
        if (GetPos(node_list_, o) == -1) {
            return true;
        }
    }
    return false;
}

SuperQueryNode::SuperQueryNode(int id, const std::vector<unsigned>& node_list, Graph *query_graph) {
    id_ = id;
    node_list_ = node_list;
    std::sort(node_list_.begin(), node_list_.end());

    for (auto u: node_list_) {
        for (auto v: node_list_) {
            if (u != v && query_graph->checkEdgeExistence(u, v)) link_info_[u].push_back(v);
        }
    }

    std::function<void(unsigned)> Dfs = [&](unsigned u) {
        // parents_.emplace_back();
        // for (auto u1 : order_) {
        //     if (query_graph->checkEdgeExistence(u, u1)) {
        //         parents_.back().push_back(u1);
        //     }
        // }
        // order_.push_back(u);
        // pos_[u] = order_.size() - 1;
        // for (auto v : link_info_[u]) {
        //     if (HaveOuterLink(v, query_graph) && GetPos(order_, v) == -1) Dfs(v);
        // }
        // for (auto v : link_info_[u]) {
        //     if (GetPos(order_, v) == -1) Dfs(v);
        // }
        
    };

    for (int i = 0; i <= 1; ++i)
    for (auto u : node_list_) {
        if (GetPos(order_, u) == -1 && (i == 1 || HaveOuterLink(u, query_graph)) ) {
            root_ids_.push_back(u);
            order_.push_back(u);
            pos_[u] = order_.size() - 1;
            parents_.emplace_back();
            std::vector<ui> ext;
            for (auto v: link_info_[u]) {
                ext.push_back(v);
            }
            for (;;) {
                int next = -1;
                bool next_have_outer = false;
                for (auto v : ext) {
                    if (GetPos(order_, v) != -1) continue;
                    bool v_have_outer = HaveOuterLink(v, query_graph);
                    if (next == -1 || (v_have_outer && !next_have_outer)) {
                        next = v;
                        next_have_outer = v_have_outer;
                    }
                }
                if (next == -1) break;
                u = next;
                order_.push_back(u);
                pos_[u] = order_.size() - 1;
                parents_.emplace_back();
                for (auto v: link_info_[u]) {
                    if (GetPos(order_, v) == -1 && GetPos(ext, v) == -1) {
                        ext.push_back(v);
                    }
                }
                for (auto u1 : order_) {
                    if (query_graph->checkEdgeExistence(u, u1)) {
                        parents_.back().push_back(u1);
                    }
                }
            }
        }
    }
}
