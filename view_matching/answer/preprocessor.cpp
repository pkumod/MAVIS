#include "preprocessor.h"

#include "encoder.h"
#include "global_vars.h"
#include "planner.h"
#include "primitive/projection.h"
#include "query_plan_generator.h"
#include "query_plan_generator1.h"
#include "rm_filter.h"

void Preprocessor::FilterOnQ(Graph* query_graph, Graph* data_graph, catalog* storage) {
    std::unique_ptr<RMFilter> pp(new RMFilter(rq_));
    pp->execute(query_graph, data_graph, storage, true);

    auto start = std::chrono::high_resolution_clock::now();
    CalcCandidateSets(storage, query_graph_, data_graph_->getVerticesCount());
    auto end = std::chrono::high_resolution_clock::now();
    double calc_can_time = std::chrono::duration<double, std::micro>(end - start).count();

    auto rapidmatch_pp_cost = (pp->filter_time_ + pp->scan_time_ + pp->semi_join_time_) / 1000. + calc_can_time;
    std::cout << "scan time(ms): " << pp->scan_time_ / 1000000. << std::endl;
    std::cout << "calc candidate time(ms): " << calc_can_time / 1000. << std::endl;
    std::cout << "rapidmatch pp cost(ms): " << rapidmatch_pp_cost / 1000. << std::endl;

    g_preprocess_cost += rapidmatch_pp_cost;
}

void Preprocessor::CalcCandidateSets(catalog* storage, const Graph* query_graph, unsigned data_vertex_count) {
    uint32_t n = query_graph->getVerticesCount();

    std::unique_ptr<projection> projection_operator(new projection(data_vertex_count));
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t u = i;
        uint32_t nbr_cnt;
        const uint32_t* nbrs = query_graph->getVertexNeighbors(u, nbr_cnt);
        for (uint32_t j = 0; j < nbr_cnt; ++j) {
            uint32_t v = nbrs[j];
            uint32_t src = u;
            uint32_t dst = v;
            uint32_t kp = 0;
            if (src > dst) {
                std::swap(src, dst);
                kp = 1;
            }

            projection_operator->execute(&storage->edge_relations_[src][dst], kp, storage->candidate_sets_[u], storage->num_candidates_[u]);
            break;
        }
    }
}

void Preprocessor::HandleNeighborEquivalenceClass(Graph* query_graph, UnionFind<ui>& rw_uf) {
    auto start = std::chrono::high_resolution_clock::now();
    aux_.first_of_nec_.resize(query_graph->getVerticesCount(), false);
    // get neighbor equivalence class
    UnionFind<ui> uf(query_graph->getVerticesCount());

#ifdef USE_NEC
    if (g_port_vertex_optimization) {
        for (int i = 0; i < query_graph->getVerticesCount(); ++i) {
            for (int j = i + 1; j < query_graph->getVerticesCount(); ++j) {
                if (query_graph->getVertexLabel(i) == query_graph->getVertexLabel(j) && query_graph->getNeighborVec(i) == query_graph->getNeighborVec(j)) {
                    uf.Union(i, j);
                }
            }
        }
    }
#endif

    std::map<ui, std::vector<ui>> nec_map;
    // group each vertex by its equivalence class
    for (int i = 0; i < query_graph->getVerticesCount(); ++i) {
        nec_map[uf.Find(i)].push_back(i);
    }
    for (auto [_, nec] : nec_map) {
        for (auto u : nec) {
            aux_.q_nec_count_[u] = nec.size();
        }
    }

    // reorder rw_order according to the neighbor equivalence class
    std::vector<ui> new_rw_order;
    for (int i = 0; i < aux_.rw_order_.size(); ++i) {
        if (aux_.rw_order_[i] == -1) continue;
        auto ru = aux_.rw_order_[i];
        if (rq_.nodes_[ru]->GetNodeNum() != 1) {
            new_rw_order.push_back(ru);
            aux_.rq_nec_count_[ru] = 1;
            auto node_list = rq_.nodes_[ru]->GetNodeList();
            for (auto node : node_list) {
                aux_.first_of_nec_[node] = true;
            }
            continue;
        }
        auto u = rq_.nodes_[ru]->GetSingleNode();
        std::vector<ui> nec = {ru};
        for (int j = i + 1; j < aux_.rw_order_.size(); ++j) {
            auto ru1 = aux_.rw_order_[j];
            if (ru1 == -1) continue;
            if (rq_.nodes_[ru1]->GetNodeNum() != 1) continue;
            auto u1 = rq_.nodes_[ru1]->GetSingleNode();
            if (uf.Find(u) == uf.Find(u1)) {
                nec.push_back(ru1);
                aux_.rw_order_[j] = -1;
                rw_uf.Union(ru, ru1);
            }
        }
        for (auto ru : nec) {
            new_rw_order.push_back(ru);
            aux_.rq_nec_count_[ru] = nec.size();
        }
        aux_.first_of_nec_[u] = true;
    }
    aux_.rw_order_ = new_rw_order;

    // update candidate sets
    for (auto [_, nec] : nec_map) {
        if (nec.size() == 1) continue;
        auto u = nec[0];
        int min_size = 1e9, min_size_id = -1;
        for (auto u : nec) {
            if (aux_.storage_->get_num_candidates(u) < min_size) {
                min_size = aux_.storage_->get_num_candidates(u);
                min_size_id = u;
            }
        }
        for (auto u : nec) {
            if (u == min_size_id) continue;
            // copy
            aux_.storage_->num_candidates_[u] = aux_.storage_->num_candidates_[min_size_id];
            memcpy(aux_.storage_->candidate_sets_[u], aux_.storage_->candidate_sets_[min_size_id], aux_.storage_->num_candidates_[u] * sizeof(unsigned));
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto handle_nec_cost = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "first of nec: " << aux_.first_of_nec_ << std::endl;
    std::cout << "handle_nec_cost(us): " << handle_nec_cost << std::endl;
}

void Preprocessor::GetNoRepeatLabel(Graph* query_graph) {
    /**
     * get no_repeat label
     */
    std::set<unsigned> label_set;
    for (int i = aux_.rw_order_.size() - 1; i >= 0; --i) {
        auto u = aux_.rw_order_[i];
        // check no repeat label
        bool no_repeat_label = true;
        const auto& nodes = rq_.nodes_[u]->GetNodeList();
        for (auto node : nodes) {
            if (label_set.find(query_graph->getVertexLabel(node)) != label_set.end()) {
                no_repeat_label = false;
                break;
            }
        }
        aux_.no_repeat_label_[u] = no_repeat_label;
        for (auto node : nodes) {
            label_set.insert(query_graph->getVertexLabel(node));
        }
    }
}

void Preprocessor::CalcParentRelatedInfo() {
    /**
     * calculate parent-related information
     */
    std::vector<bool> vis(rq_.nodes_.size(), false);
    aux_.parents_.resize(rq_.nodes_.size());
    aux_.parent_edges_.resize(rq_.nodes_.size());
    aux_.parent_rev_edges_.resize(rq_.nodes_.size());
    for (auto u : aux_.rw_order_) {
        vis[u] = true;
        for (auto edge : rq_.link_[u]) {
            auto v = edge->dst_;
            if (vis[v]) {
                aux_.parents_[u].push_back(v);
                aux_.parent_edges_[u].push_back(edge);
                for (auto rev_edge : rq_.link_[v]) {
                    if (rev_edge->dst_ == u) {
                        aux_.parent_rev_edges_[u].push_back(rev_edge);
                        break;
                    }
                }
            }
        }
    }
}

void Preprocessor::Exec() {
    aux_.query_graph_ = query_graph_;
    aux_.data_graph_ = data_graph_;

    auto storage = new catalog(query_graph_, data_graph_);
    aux_.storage_.reset(storage);
    FilterOnQ(query_graph_, data_graph_, storage);

    auto start = std::chrono::high_resolution_clock::now();

    /*
     * get rw_order
     */
    // input_rw_order_ = {5, 3, 0, 1, 2, 4, 9, 8, 6, 7, 10, 11, 13, 12};
    if (input_rw_order_.empty()) {
        // GetOrder3(rq, aux);
        // GetOrder2(rq, aux);
        // GetOrder5();
        // GetOrder(rq, aux);
        Planner planner(rq_, aux_, query_graph_, data_graph_);
        planner.GetOrder6();
        // planner.GetOrder4_ori();
        // planner.GetOrderOPV();
    } else {
        aux_.rw_order_ = std::move(input_rw_order_);
    }

    aux_.no_repeat_label_.resize(rq_.nodes_.size(), false);
    aux_.q_nec_count_.resize(query_graph_->getVerticesCount(), 0);
    aux_.rq_nec_count_.resize(aux_.rw_order_.size(), 1);
    aux_.query_node_no_.resize(query_graph_->getVerticesCount());
    aux_.parent_query_nodes_.resize(query_graph_->getVerticesCount());

    UnionFind<ui> rw_uf(aux_.rw_order_.size());
    HandleNeighborEquivalenceClass(query_graph_, rw_uf);

    CalcParentRelatedInfo();

    GetNoRepeatLabel(query_graph_);

    /**
     * get port vertices
     */
    aux_.port_vertices_.resize(rq_.nodes_.size());
    aux_.non_port_vertices_.resize(rq_.nodes_.size());
    if (g_port_vertex_optimization) {
        for (int i = aux_.rw_order_.size() - 1; i >= 0; --i) {
            auto ru = aux_.rw_order_[i];
            auto& port_vertices = aux_.port_vertices_[ru];
            auto& non_port_vertices = aux_.non_port_vertices_[ru];

            for (int j = 0; j < rq_.nodes_[ru]->GetNodeNum(); ++j) {
                auto u = rq_.nodes_[ru]->GetInnerOrder()[j];
                bool is_port = false;
                for (int k = i + 1; k < aux_.rw_order_.size(); ++k) {
                    auto rv = aux_.rw_order_[k];

                    if (rw_uf.Find(ru) == rw_uf.Find(rv)) continue;

                    for (int l = 0; l < rq_.nodes_[rv]->GetNodeNum(); ++l) {
                        auto v = rq_.nodes_[rv]->GetInnerOrder()[l];
                        if (query_graph_->checkEdgeExistence(u, v) ||
                            query_graph_->getVertexLabel(u) == query_graph_->getVertexLabel(v)) {
                            is_port = true;
                            break;
                        }
                    }
                    if (is_port) break;
                }
                if (is_port) {
                    port_vertices.push_back(j);
                } else {
                    non_port_vertices.push_back(j);
                }
            }

            std::cout << "rw_node " << ru << " port vertices: ";
            for (auto idx : port_vertices) {
                std::cout << rq_.nodes_[ru]->GetInnerOrder()[idx] << " ";
            }
            std::cout << std::endl;
        }
    }
    else {
        for (int i = 0; i < aux_.rw_order_.size() - 1; ++i) {
            auto ru = aux_.rw_order_[i];
            auto& port_vertices = aux_.port_vertices_[ru];

            for (int j = 0; j < rq_.nodes_[ru]->GetNodeNum(); ++j) {
                port_vertices.push_back(j);
            }
        }
    }

    /**
     * calculate order in query graph
     */
    for (auto rw_node_id : aux_.rw_order_) {
        auto rw_node = rq_.nodes_[rw_node_id];
        for (auto node : rw_node->GetInnerOrder()) {
            aux_.query_node_order_.push_back(node);
        }
    }
    for (int i = 0; i < aux_.query_node_order_.size(); ++i) {
        aux_.query_node_no_[aux_.query_node_order_[i]] = i;
    }
    for (int i = 0; i < aux_.query_node_order_.size(); ++i) {
        auto u = aux_.query_node_order_[i];
        for (int j = 0; j < i; ++j) {
            auto v = aux_.query_node_order_[j];
            if (rq_.rw_node_no_[u] == rq_.rw_node_no_[v]) continue;
            if (query_graph_->checkEdgeExistence(u, v)) {
                aux_.parent_query_nodes_[u].push_back(v);
            }
        }
    }

    /**
     * calculate enumerate method
     */
    aux_.enum_methods_.resize(rq_.nodes_.size());
    for (int i = 0; i < aux_.rw_order_.size(); ++i) {
        auto u = aux_.rw_order_[i];
        if (i == 0) {
            if (rq_.nodes_[u]->type_ == RewriteQuery::Type::MATERIALIZED)
                aux_.enum_methods_[u] = EnumMethod::MatEdge;
            else
                aux_.enum_methods_[u] = EnumMethod::NormalNode;
            continue;
        }

        // test if all parent edges are mat edges with high coverage
        bool all_mat_edge = false;
        if (rq_.nodes_[u]->type_ == RewriteQuery::Type::MATERIALIZED) {
            all_mat_edge = true;
            for (auto edge : aux_.parent_edges_[u]) {
                if (edge->type_ != RewriteQuery::Type::MATERIALIZED) {
                    all_mat_edge = false;
                    break;
                } else {
                    RewriteQuery::MatEdge* mat_edge = (RewriteQuery::MatEdge*)edge;
                    auto coverage = mat_edge->covered_edge_num_ / (double)(mat_edge->covered_edge_num_ + mat_edge->patch_edges_.size());
                    if (coverage < 0.66) {
                        all_mat_edge = false;
                        break;
                    }
                }
            }
        }

        // test if the rw_node is considered as normal node
        bool normal_node = false;
        if (rq_.nodes_[u]->GetNodeNum() == 1) {
            if (rq_.nodes_[u]->type_ == RewriteQuery::Type::NORMAL) {
                normal_node = true;
            } else {  // if the rw node is mat node with single query node, check its parent edges to decide whether it is handled as normal node
                if (!all_mat_edge) {
                    normal_node = true;
                } else {
                    // auto query_node = rq_.nodes_[u]->GetNodeList()[0];
                    // if (aux_.parent_query_nodes_[query_node].size() - aux_.parent_edges_[u].size() <= 1) {
                    //     // it's benificial if super edges are more than query edges
                    //     normal_node = true;
                    // }
                }
            }
        }

        // decide enumerate method
        if (normal_node) {
            aux_.enum_methods_[u] = EnumMethod::NormalNode;
        } else {
            if (all_mat_edge) {
                aux_.enum_methods_[u] = EnumMethod::MatEdge;
                for (auto edge : aux_.parent_rev_edges_[u]) {
                    auto parent_rev_mat_edge = (RewriteQuery::MatEdge*)edge;
                    auto& pa_rw_node_id = parent_rev_mat_edge->src_;
                    auto& se = parent_rev_mat_edge->super_edge_cand_;
                    auto d = parent_rev_mat_edge->d_;
                    if (aux_.enum_methods_[pa_rw_node_id] == EnumMethod::NormalNode) {
                        aux_.enum_methods_[u] = EnumMethod::Trie;
                        break;
                    } else if (se->all_materialized_[d] == false) {
                        aux_.enum_methods_[u] = EnumMethod::Mix;
                    }
                }
            } else {
                aux_.enum_methods_[u] = EnumMethod::Trie;
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    g_preprocess_cost += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::unique_ptr<Encoder> en(new Encoder(query_graph_, data_graph_->getVerticesCount(), aux_, rq_));
    en->execute();
    g_rapidmatch_encode_cost = en->encoding_time_;
    g_encode_cost = g_rapidmatch_encode_cost / 1000.0;

    PrintInfo();
}

void Preprocessor::PrintInfo() {
    std::cout << "rw_order: " << aux_.rw_order_ << std::endl;
    std::cout << "no repeat label: ";
    for (auto u : aux_.rw_order_) {
        std::cout << aux_.no_repeat_label_[u] << ", ";
    }
    std::cout << std::endl;
    std::cout << "query_node_order: " << aux_.query_node_order_ << std::endl;
    std::cout << "query_node_no: " << aux_.query_node_no_ << std::endl;
    std::cout << "parents: " << aux_.parents_ << std::endl;
    for (int i = 0; i < aux_.rw_order_.size(); ++i) {
        auto u = aux_.rw_order_[i];
        std::cout << u << ":";
        switch (aux_.enum_methods_[u]) {
            case EnumMethod::NormalNode:
                std::cout << "NormalNode";
                break;
            case EnumMethod::MatEdge:
                std::cout << "MatEdge";
                break;
            case EnumMethod::Mix:
                std::cout << "Mix";
                break;
            case EnumMethod::Trie:
                std::cout << "Trie";
                break;
        }
        std::cout << " ";
    }
    std::cout << std::endl;
    std::cout << "nec_count: " << aux_.q_nec_count_ << std::endl;
    std::cout << "nec_count1: " << aux_.rq_nec_count_ << std::endl;
    std::cout << "port query nodes: ";
    for (int i = 0; i < aux_.rw_order_.size(); ++i) {
        auto node_id = aux_.rw_order_[i];
        for (auto idx : aux_.port_vertices_[node_id]) {
            std::cout << rq_.nodes_[node_id]->GetInnerOrder()[idx] << " ";
        }
    }
    std::cout << std::endl;
}