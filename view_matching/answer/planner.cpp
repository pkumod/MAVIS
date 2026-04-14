#include "planner.h"

#include "query_plan_generator.h"
#include "query_plan_generator1.h"

int Planner::GetCandidateSize(unsigned node_id) {
    auto rw_node = rq_.nodes_[node_id];
    ui ret;
    if (rw_node->type_ == RewriteQuery::Type::NORMAL) {
        auto normal_node = rw_node;
        ret = aux_.storage_->get_num_candidates(normal_node->node_id_);
    } else {
        auto mat_node = rw_node;
        ret = mat_node->sdn_->GetEmbeddingNum();
        // ret = pow(ret, -mat_node->GetNodeNum() / 2);
    }
    return ret;
}

void Planner::GetOrder() {
    Catalog1 storage;
    unsigned node_num = rq_.nodes_.size();
    storage.node_num = node_num;
    storage.num_candidates_ = new unsigned[node_num];
    storage.edge_relation_cardinality_ = new unsigned*[node_num];
    // calc edge_relation_cardinality
    for (int i = 0; i < node_num; ++i) {
        storage.edge_relation_cardinality_[i] = new unsigned[node_num];
    }
    // calc num_candidates
    for (int i = 0; i < node_num; ++i) {
        storage.num_candidates_[i] = GetCandidateSize(i);
    }

    std::vector<std::vector<unsigned>> orders;
    query_plan_generator1::generate_query_plan_with_nd(rq_.rw_graph_.get(), &storage, orders);
    aux_.rw_order_ = orders[0];
}

void Planner::BuildCoreGraph(Graph* g, Graph*& core_g, std::vector<unsigned>& core_vertices) {
    std::vector<std::tuple<VertexID, LabelID, ui>> v_tuples;
    std::vector<std::tuple<VertexID, VertexID>> e_tuples;
    for (int i = 0; i < g->getVerticesCount(); ++i) {
        if (g->getCoreValue(i) >= 2) {
            core_vertices.push_back(i);
        }
    }
    if (core_vertices.empty()) return;
    core_g = new Graph(true);
    std::vector<int> degrees(core_vertices.size(), 0);
    for (int i = 0; i < core_vertices.size(); ++i) {
        auto u = core_vertices[i];
        for (int j = i + 1; j < core_vertices.size(); ++j) {
            auto v = core_vertices[j];
            if (g->checkEdgeExistence(u, v)) {
                e_tuples.push_back(std::make_tuple(i, j));
                degrees[i]++;
                degrees[j]++;
            }
        }
    }
    for (int i = 0; i < core_vertices.size(); ++i) {
        v_tuples.push_back(std::make_tuple(i, 0, degrees[i]));
    }
    core_g->loadGraphFromTuples(v_tuples, e_tuples);
}

void Planner::GetCoreOrder() {
    std::unique_ptr<Graph> core_g(nullptr);
    Graph* core_g_rp = nullptr;
    std::vector<unsigned> core_vertices;
    BuildCoreGraph(rq_.rw_graph_.get(), core_g_rp, core_vertices);
    core_g.reset(core_g_rp);
    if (core_vertices.size() == 0) return;
    Catalog1 storage;
    unsigned node_num = core_vertices.size();
    storage.node_num = node_num;
    storage.num_candidates_ = new unsigned[node_num];
    storage.edge_relation_cardinality_ = new unsigned*[node_num];
    // calc edge_relation_cardinality
    for (int i = 0; i < node_num; ++i) {
        storage.edge_relation_cardinality_[i] = new unsigned[node_num];
    }
    // calc num_candidates
    for (int i = 0; i < node_num; ++i) {
        storage.num_candidates_[i] = GetCandidateSize(core_vertices[i]);
    }
    std::vector<std::vector<unsigned>> orders;
    query_plan_generator1::generate_query_plan_with_nd(core_g.get(), &storage,
                                                       orders);
    for (auto node : orders[0]) {
        aux_.rw_order_.push_back(core_vertices[node]);
    }
}

void Planner::GetOrder2() {
    std::vector<std::vector<unsigned>> orders;
    query_plan_generator::generate_query_plan_with_nd(aux_.query_graph_, aux_.storage_.get(), orders);
    std::cout << "rapidmatch order cost(ms): " << query_plan_generator::ordering_time_ / 1000000 << std::endl;
    auto& order = orders[0];
    std::cout << "rapidmatch order: " << order << std::endl;
    std::vector<bool> vis(aux_.query_graph_->getVerticesCount(), false);
    aux_.rw_order_.reserve(rq_.nodes_.size());
    for (auto u : order) {
        if (vis[u]) continue;
        auto rw_node_id = rq_.rw_node_no_[u];
        aux_.rw_order_.push_back(rw_node_id);
        const auto& nodes = rq_.nodes_[rw_node_id]->GetNodeList();
        for (auto node : nodes) {
            vis[node] = true;
        }
    }
}

void Planner::GetOrder3() {
    GetCoreOrder();

    // add rest vertices to rw_order
    std::vector<bool> vis(rq_.rw_graph_->getVerticesCount(), false);
    std::vector<unsigned> degree(rq_.rw_graph_->getVerticesCount(), 0);
    std::priority_queue<std::tuple<unsigned, unsigned, unsigned>>
        pq;  // type, candidate_size, node_id
    std::vector<int> reverse_order;
    for (int i = 0; i < rq_.rw_graph_->getVerticesCount(); ++i) {
        degree[i] = rq_.link_[i].size();
        if (degree[i] == 1) {
            pq.push(
                std::make_tuple(rq_.nodes_[i]->type_ == RewriteQuery::NORMAL, 0, i));
            vis[i] = true;
        }
    }
    while (pq.empty() == false) {
        auto [type, candidate_size, node_id] = pq.top();
        pq.pop();
        reverse_order.push_back(node_id);
        for (auto edge_info : rq_.link_[node_id]) {
            auto v = edge_info->dst_;
            if (vis[v]) continue;
            degree[v]--;
            if (degree[v] == 1 || degree[v] == 0) {
                pq.push(
                    std::make_tuple(rq_.nodes_[v]->type_ == RewriteQuery::NORMAL, 0, v));
                vis[v] = true;
            }
        }
    }
    for (int i = reverse_order.size() - 1; i >= 0; --i) {
        aux_.rw_order_.push_back(reverse_order[i]);
    }
}

void Planner::GetOrder4() {
    int max_degree = 0;
    int first_node = -1;
    int min_can_size = 1e9;
    for (int i = 0; i < rq_.rw_graph_->getVerticesCount(); ++i) {
        if (rq_.rw_graph_->getCoreValue(i) >= 2) {
            if (rq_.link_[i].size() > max_degree || (rq_.link_[i].size() == max_degree && GetCandidateSize(i) < min_can_size)) {
                max_degree = rq_.link_[i].size();
                min_can_size = GetCandidateSize(i);
                first_node = i;
            }
        }
    }
    if (first_node == -1) {
        for (int i = 0; i < rq_.rw_graph_->getVerticesCount(); ++i) {
            if (rq_.link_[i].size() > max_degree || (rq_.link_[i].size() == max_degree && GetCandidateSize(i) < min_can_size)) {
                max_degree = rq_.link_[i].size();
                min_can_size = GetCandidateSize(i);
                first_node = i;
            }
        }
    }
    aux_.rw_order_.push_back(first_node);
    std::vector<bool> vis(rq_.rw_graph_->getVerticesCount(), false);
    std::vector<int> ext;
    for (auto edge_info : rq_.link_[first_node]) {
        ext.push_back(edge_info->dst_);
    }
    vis[first_node] = true;
    for (int i = 1; i < rq_.rw_graph_->getVerticesCount(); ++i) {
        double max_bn_cnt = 0;
        int max_degree = 0;
        int next_node = -1;
        for (auto j : ext) {
            if (vis[j]) continue;
            double bn_cnt = 0;
            for (auto edge_info : rq_.link_[j]) {
                if (vis[edge_info->dst_]) {
                    bn_cnt++;
                }
            }
            for (int k = 0; k < rq_.rw_graph_->getVerticesCount(); ++k) {
                if (vis[k]) {
                    int same_label_cnt = 0;
                    for (auto u : rq_.nodes_[k]->GetNodeList()) {
                        for (auto v : rq_.nodes_[j]->GetNodeList()) {
                            if (aux_.query_graph_->getVertexLabel(u) == aux_.query_graph_->getVertexLabel(v)) {
                                same_label_cnt++;
                            }
                        }
                    }
                    bn_cnt += same_label_cnt * 0.001;
                }
            }
            if (bn_cnt > max_bn_cnt || (bn_cnt == max_bn_cnt && rq_.link_[j].size() > max_degree)) {
                max_bn_cnt = bn_cnt;
                max_degree = rq_.link_[j].size();
                next_node = j;
            }
        }
        assert(next_node != -1);
        aux_.rw_order_.push_back(next_node);
        vis[next_node] = true;
        for (auto edge_info : rq_.link_[next_node]) {
            if (vis[edge_info->dst_] == false) {
                ext.push_back(edge_info->dst_);
            }
        }
    }
}

void Planner::GetOrder4_ori() {
    int core_cnt = rq_.rw_graph_->get2CoreSize();

    int max_degree = 0;
    int first_node = -1;
    int min_can_size = 1e9;
    for (int i = 0; i < rq_.rw_graph_->getVerticesCount(); ++i) {
        if (core_cnt == 0 || rq_.rw_graph_->getCoreValue(i) >= 2) {
            if (rq_.link_[i].size() > max_degree ||
                (rq_.link_[i].size() == max_degree && GetCandidateSize(i) < min_can_size)) {
                max_degree = rq_.link_[i].size();
                min_can_size = GetCandidateSize(i);
                first_node = i;
            }
        }
    }

    assert(first_node != -1);

    if (core_cnt > 0) core_cnt--;

    std::vector<bool> vis(rq_.rw_graph_->getVerticesCount(), false);

    aux_.rw_order_.push_back(first_node);
    vis[first_node] = true;

    for (int i = 1; i < rq_.rw_graph_->getVerticesCount(); ++i) {
        int max_bn_cnt = 0;
        int max_degree = 0;
        int next_node = -1;
        for (int j = 0; j < rq_.rw_graph_->getVerticesCount(); ++j) {
            if (vis[j]) continue;
            if (core_cnt > 0 && rq_.rw_graph_->getCoreValue(j) < 2) continue;
            int bn_cnt = 0;
            for (auto edge_info : rq_.link_[j]) {
                if (vis[edge_info->dst_]) {
                    bn_cnt++;
                }
            }
            if (bn_cnt > max_bn_cnt || (bn_cnt == max_bn_cnt && rq_.link_[j].size() > max_degree)) {
                max_bn_cnt = bn_cnt;
                max_degree = rq_.link_[j].size();
                next_node = j;
            }
        }

        assert(next_node != -1);

        aux_.rw_order_.push_back(next_node);
        vis[next_node] = true;

        if (core_cnt > 0) core_cnt--;
    }
}

void Planner::GetOrder5() {
    std::vector<std::vector<ui>> top_link, same_label_link;
    top_link.resize(rq_.nodes_.size());
    same_label_link.resize(rq_.nodes_.size());
    for (auto i = 0; i < rq_.nodes_.size(); ++i) {
        same_label_link[i].resize(rq_.nodes_.size(), 0);
        for (auto edge : rq_.link_[i]) {
            top_link[i].push_back(edge->dst_);
        }
    }
    for (auto i = 0; i < rq_.nodes_.size(); ++i) {
        for (auto j = 0; j < rq_.nodes_.size(); ++j) {
            ui& same_label_cnt = same_label_link[i][j];
            for (auto u : rq_.nodes_[i]->GetNodeList()) {
                for (auto v : rq_.nodes_[j]->GetNodeList()) {
                    if (query_graph_->getVertexLabel(u) == query_graph_->getVertexLabel(v)) {
                        same_label_cnt++;
                    }
                }
            }
        }
    }

    std::vector<double> degree(rq_.nodes_.size(), 0.0);
    for (int i = 0; i < rq_.nodes_.size(); ++i) {
        degree[i] = top_link[i].size();
        for (int j = 0; j < rq_.nodes_.size(); ++j) {
            degree[i] += same_label_link[i][j] * 0.1;
        }
    }

    // find max_degree node, if tie, find min candidate size
    double max_degree = 0;
    int first_node = -1;
    int min_can_size = 1e9;
    for (int i = 0; i < rq_.nodes_.size(); ++i) {
        if (degree[i] > max_degree || (degree[i] == max_degree && GetCandidateSize(i) < min_can_size)) {
            max_degree = degree[i];
            min_can_size = GetCandidateSize(i);
            first_node = i;
        }
    }

    aux_.rw_order_.push_back(first_node);
    std::vector<ui> nbrs;
    std::vector<bool> vis(rq_.nodes_.size(), false);
    vis[first_node] = true;
    for (auto node : top_link[first_node]) {
        nbrs.push_back(node);
        degree[node]--;
    }
    for (int i = 0; i < rq_.nodes_.size(); ++i) {
        degree[i] -= same_label_link[first_node][i] * 0.1;
    }

    for (int i = 1; i < rq_.nodes_.size(); ++i) {
        // find max degree in nbrs
        double max_degree = -1;
        int next_node = -1;
        int min_can_size = 1e9;
        for (auto u : nbrs) {
            if (vis[u]) continue;
            if (degree[u] > max_degree || (degree[u] == max_degree && GetCandidateSize(u) < min_can_size)) {
                max_degree = degree[u];
                min_can_size = GetCandidateSize(u);
                next_node = u;
            }
        }
        assert(next_node != -1);
        aux_.rw_order_.push_back(next_node);
        vis[next_node] = true;
        // update nbrs
        for (auto node : top_link[next_node]) {
            degree[node]--;
            if (vis[node]) continue;
            nbrs.push_back(node);
        }
        for (int j = 0; j < rq_.nodes_.size(); ++j) {
            degree[j] -= same_label_link[next_node][j] * 0.1;
        }
        std::cout << "step " << i << ", next_node: " << next_node << ", nbrs size: " << nbrs.size() << std::endl;
        std::cout << "degree: " << degree << std::endl;
        std::cout << "cand_size: ";
        for (int j = 0; j < rq_.nodes_.size(); ++j) {
            std::cout << GetCandidateSize(j) << " ";
        }
        std::cout << std::endl;
    }
}

int Planner::Cmp(double a) {
    return fabs(a) < eps ? 0 : (a > 0 ? 1 : -1);
}

void Planner::GetOrder6() {
    if (rq_.rw_graph_->core_table_ == nullptr) {
        rq_.rw_graph_->buildCoreTable();
    }

    int core_cnt = rq_.rw_graph_->get2CoreSize();

    std::vector<std::vector<double>> dpd_mat;  // dependency matrix
    std::vector<double> dpd_sum(rq_.nodes_.size(), 0.0);
    dpd_mat.resize(rq_.nodes_.size());
    for (int i = 0; i < rq_.nodes_.size(); ++i) {
        dpd_mat[i].resize(rq_.nodes_.size(), 0.0);
    }

    double label_coef = 0.01, edge_coef = 1.0;
    for (int ru = 0; ru < rq_.nodes_.size(); ++ru) {
        for (int rv = 0; rv < rq_.nodes_.size(); ++rv) {
            if (ru == rv) continue;
            auto ru_node_list = rq_.nodes_[ru]->GetNodeList();
            auto rv_node_list = rq_.nodes_[rv]->GetNodeList();
            for (auto u : ru_node_list) {
                for (auto v : rv_node_list) {
                    if (query_graph_->checkEdgeExistence(u, v)) {
                        dpd_mat[ru][rv] += edge_coef;
                    }
                    if (query_graph_->getVertexLabel(u) == query_graph_->getVertexLabel(v)) {
                        dpd_mat[ru][rv] += label_coef;
                    }
                }
            }

            dpd_sum[ru] += dpd_mat[ru][rv];
        }
    }

    // find first node
    int first_node = -1;
    double max_score = -1.0;
    for (int ru = 0; ru < rq_.nodes_.size(); ++ru) {
        if (core_cnt > 0 && rq_.rw_graph_->getCoreValue(ru) < 2) continue;
        if (Cmp(max_score - dpd_sum[ru]) < 0 || (Cmp(max_score - dpd_sum[ru]) == 0 && GetCandidateSize(ru) < GetCandidateSize(first_node))) {
            max_score = dpd_sum[ru];
            first_node = ru;
        }
    }
    aux_.rw_order_.push_back(first_node);
    for (int rv = 0; rv < rq_.nodes_.size(); ++rv) {
        dpd_sum[rv] -= dpd_mat[first_node][rv];
    }
    core_cnt -= (rq_.rw_graph_->getCoreValue(first_node) >= 2) ? 1 : 0;

    std::vector<ui> ext;
    ui len;
    auto nbrs = rq_.rw_graph_->getVertexNeighbors(first_node, len);
    for (int j = 0; j < len; ++j) {
        ext.push_back(nbrs[j]);
    }

    for (;;) {
        if (aux_.rw_order_.size() == rq_.nodes_.size()) break;
        // find next node
        int next_node = -1;
        double max_score = -1.0;
        for (auto ru : ext) {
            if (std::find(aux_.rw_order_.begin(), aux_.rw_order_.end(), ru) != aux_.rw_order_.end()) continue;
            if (core_cnt > 0 && rq_.rw_graph_->getCoreValue(ru) < 2) continue;
            if (Cmp(max_score - dpd_sum[ru]) < 0 || (Cmp(max_score - dpd_sum[ru]) == 0 && GetCandidateSize(ru) < GetCandidateSize(next_node))) {
                max_score = dpd_sum[ru];
                next_node = ru;
            }
        }

        // std::cout << "====================" << std::endl;
        // std::cout << "ext: " << ext << std::endl;
        // std::cout << "core_cnt: " << core_cnt << std::endl;
        // std::cout << "dpd_sum: " << dpd_sum << std::endl;
        // std::cout << "max_score: " << max_score << std::endl;
        // std::cout << "next_node: " << next_node << std::endl;
        // std::cout << "candidate size: " ;
        // for (int i = 0; i < rq_.nodes_.size(); ++i) {
        //     std::cout << GetCandidateSize(i) << " ";
        // }
        // std::cout << std::endl;

        aux_.rw_order_.push_back(next_node);
        for (int rv = 0; rv < rq_.nodes_.size(); ++rv) {
            dpd_sum[rv] -= dpd_mat[next_node][rv];
        }
        core_cnt -= (rq_.rw_graph_->getCoreValue(next_node) >= 2) ? 1 : 0;

        ui len;
        auto nbrs = rq_.rw_graph_->getVertexNeighbors(next_node, len);
        for (int j = 0; j < len; ++j) {
            ext.push_back(nbrs[j]);
        }
    }
}

void Planner::GetOrderOPV() {
    if (rq_.rw_graph_->core_table_ == nullptr) {
        rq_.rw_graph_->buildCoreTable();
    }

    int core_cnt = rq_.rw_graph_->get2CoreSize();

    double label_coef = 0.01, edge_coef = 1.0;

    double max_score = 0.0;
    int first_node = -1;
    for (int i = 0; i < rq_.nodes_.size(); ++i) {
        if (core_cnt > 0 && rq_.rw_graph_->getCoreValue(i) < 2) continue;
        int out_port_num = 0;
        double out_edge_sum = 0.0;

        auto& node_list = rq_.nodes_[i]->GetNodeList();
        for (auto u : node_list) {
            bool is_out_port = false;
            for (int v = 0; v < query_graph_->getVerticesCount(); ++v) {
                if (std::find(node_list.begin(), node_list.end(), v) != node_list.end()) continue;
                // same link contribution
                if (query_graph_->checkEdgeExistence(u, v)) {
                    out_edge_sum += edge_coef;
                    is_out_port = true;
                }
                // same label contribution
                if (query_graph_->getVertexLabel(u) == query_graph_->getVertexLabel(v)) {
                    out_edge_sum += label_coef;
                    is_out_port = true;
                }
            }
            out_port_num += is_out_port ? 1 : 0;
        }

        double score = out_edge_sum / out_port_num;
        if (Cmp(score - max_score) > 0 || 
            (Cmp(score - max_score) == 0 && GetCandidateSize(i) < GetCandidateSize(first_node))) {
            max_score = score;
            first_node = i;
        }
    }

    if (core_cnt > 0) core_cnt--;
    aux_.rw_order_.push_back(first_node);
    std::vector<ui> ext;
    ui len;
    auto nbrs = rq_.rw_graph_->getVertexNeighbors(first_node, len);
    for (int j = 0; j < len; ++j) {
        ext.push_back(nbrs[j]);
    }

    std::vector<bool> vis(query_graph_->getVerticesCount(), false);
    for (auto node : rq_.nodes_[first_node]->GetNodeList()) {
        vis[node] = true;
    }

    for (int i = 1; i < rq_.nodes_.size(); ++i) {
        int next_node = -1;
        double max_score = -1.0;
        for (auto u : ext) {
            if (std::find(aux_.rw_order_.begin(), aux_.rw_order_.end(), u) != aux_.rw_order_.end()) continue;
            if (core_cnt > 0 && rq_.rw_graph_->getCoreValue(u) < 2) continue;

            int out_port_num = 0;
            double out_edge_sum = 0.0;

            auto& node_list = rq_.nodes_[u]->GetNodeList();
            for (auto v : node_list) {
                bool is_out_port = false;
                for (int w = 0; w < query_graph_->getVerticesCount(); ++w) {
                    if (std::find(node_list.begin(), node_list.end(), w) != node_list.end()) continue;
                    if (vis[w]) continue;
                    // same link contribution
                    if (query_graph_->checkEdgeExistence(v, w)) {
                        out_edge_sum += edge_coef;
                        is_out_port = true;
                    }
                    // same label contribution
                    if (query_graph_->getVertexLabel(v) == query_graph_->getVertexLabel(w)) {
                        out_edge_sum += label_coef;
                        is_out_port = true;
                    }
                }
                out_port_num += is_out_port ? 1 : 0;
            }

            double score = 0.0;
            if (out_port_num > 0) {
                score = out_edge_sum / out_port_num;
            }
            if (Cmp(score - max_score) > 0 || 
                (Cmp(score - max_score) == 0 && GetCandidateSize(u) < GetCandidateSize(next_node))) {
                max_score = score;
                next_node = u;
            }

        }


        assert(next_node != -1);
        aux_.rw_order_.push_back(next_node);
        core_cnt -= (rq_.rw_graph_->getCoreValue(next_node) >= 2) ? 1 : 0;

        ui len;
        auto nbrs = rq_.rw_graph_->getVertexNeighbors(next_node, len);
        for (int j = 0; j < len; ++j) {
            ext.push_back(nbrs[j]);
        }
        for (auto node : rq_.nodes_[next_node]->GetNodeList()) {
            vis[node] = true;
        }
    }
}
