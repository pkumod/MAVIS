#include "subgraph_filter.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <set>

#include "MyUtils.h"
#include "preprocessor1.h"
#include "pretty_print.h"
#include "utility/primitive/nlf_filter.h"
#include "utility/primitive/projection.h"

bool CandidateSpace::CheckEdgeExistence(ui qu, ui qv, ui du, ui dv) {
    int u_pos = GetPosInOrderList(vcand_[qu], du);
    if (u_pos == -1) return false;
    int v_pos = GetPosInOrderList(ecand_[qu][qv][u_pos], dv);
    return v_pos != -1;
}

void CandidateSpace::GetCandidateEdges(unsigned u, unsigned v, std::vector<Edge>& ret) {
    ret.clear();
    for (int i = 0; i < vcand_[u].size(); ++i) {
        auto u_cand = vcand_[u][i];
        for (auto v_cand : ecand_[u][v][i]) {
            ret.emplace_back(u_cand, v_cand);
        }
    }
}

void CandidateSpace::Dump(std::string file_name) {
    std::ofstream fout(file_name);
    Dump(fout);
    fout.close();
}

void CandidateSpace::Dump(std::ostream& os) {
    os << vcand_.size() << "\n";
    for (int i = 0; i < vcand_.size(); ++i) {
        os << i << " " << vcand_[i].size() << " ";
        for (auto v : vcand_[i]) {
            os << v << " ";
        }
        os << "\n";
    }
    int edge_num = 0;
    for (int u = 0; u < ecand_.size(); ++u) {
        for (int v = 0; v < ecand_[u].size(); ++v) {
            if (!ecand_[u][v].empty()) {
                edge_num++;
            }
        }
    }
    os << edge_num << "\n";
    for (int u = 0; u < ecand_.size(); ++u) {
        for (int v = 0; v < ecand_[u].size(); ++v) {
            if (ecand_[u][v].empty()) continue;
            os << u << " " << v << "\n";
            for (int i = 0; i < vcand_[u].size(); ++i) {
                os << ecand_[u][v][i].size() << " ";
                for (auto x : ecand_[u][v][i]) {
                    os << x << " ";
                }
                os << "\n";
            }
        }
    }
}

void CandidateSpace::Load(std::string file_name) {
    std::ifstream fin(file_name);
    if (!fin.is_open()) {
        std::cerr << "Error opening file: " << file_name << std::endl;
        return;
    }
    Load(fin);
    fin.close();
}

void CandidateSpace::Load(std::istream& is) {
    int vcand_size;
    is >> vcand_size;
    vcand_.resize(vcand_size);
    for (int i = 0; i < vcand_size; ++i) {
        int cand_size;
        is >> i >> cand_size;
        vcand_[i].resize(cand_size);
        for (int j = 0; j < cand_size; ++j) {
            is >> vcand_[i][j];
        }
    }
    int edge_num;
    is >> edge_num;
    ecand_.resize(vcand_size);
    for (int i = 0; i < vcand_size; ++i) {
        ecand_[i].resize(vcand_size);
    }
    for (int e = 0; e < edge_num; ++e) {
        int u, v;
        is >> u >> v;
        ecand_[u][v].resize(vcand_[u].size());
        for (int i = 0; i < vcand_[u].size(); ++i) {
            int cand_size;
            is >> cand_size;
            ecand_[u][v][i].resize(cand_size);
            for (int j = 0; j < cand_size; ++j) {
                is >> ecand_[u][v][i][j];
            }
        }
    }
}

unsigned long long CandidateSpace::MemoryCost() {
    ui ret = 0;
    ret += sizeof(ui) * vcand_.size();  // vcand_ vector overhead
    for (const auto& vec : vcand_) {
        ret += sizeof(ui) * vec.size();  // each vcand_ inner vector
    }
    ret += sizeof(ui) * ecand_.size();  // ecand_ vector overhead
    for (const auto& vec2d : ecand_) {
        ret += sizeof(ui) * vec2d.size();  // each ecand_ first dimension
        for (const auto& vec2d_inner : vec2d) {
            ret += sizeof(ui) * vec2d_inner.size();  // each ecand_ second dimension
            for (const auto& vec1d : vec2d_inner) {
                ret += sizeof(ui) * vec1d.size();  // each ecand_ third dimension
            }
        }
    }
    return ret;
}

bool CheckIntersection(const ui* a, ui an, const ui* b, ui bn) {
    double cost_binary_search1 = an * log2(bn);
    double cost_binary_search2 = bn * log2(an);
    double cost_merge = an + bn;
    if (cost_binary_search1 <= std::min(cost_binary_search2, cost_merge)) {
        for (ui i = 0; i < an; ++i) {
            if (std::binary_search(b, b + bn, a[i])) {
                return true;
            }
        }
        return false;
    } else if (cost_binary_search2 <= std::min(cost_binary_search1, cost_merge)) {
        for (ui i = 0; i < bn; ++i) {
            if (std::binary_search(a, a + an, b[i])) {
                return true;
            }
        }
        return false;
    } else {
        ui i = 0, j = 0;
        while (i < an && j < bn) {
            if (a[i] == b[j]) {
                return true;
            } else if (a[i] < b[j]) {
                ++i;
            } else {
                ++j;
            }
        }
        return false;
    }
    assert(0);
    return false;
}

void SubgraphFilter::Prune(unsigned u, unsigned v, std::vector<std::vector<unsigned>>& candidate_sets) {
    auto& u_cand = candidate_sets[u];
    auto& v_cand = candidate_sets[v];
    double aggregate_plan_cost;
    double check_edge_plan_cost;
    int u_deg_sum = 0, v_deg_sum = 0;
    for (auto v : u_cand) {
        u_deg_sum += data_graph_->getVertexDegree(v);
    }
    aggregate_plan_cost = QuickLog2(u_cand.size()) * u_deg_sum;
    for (auto v : v_cand) {
        v_deg_sum += data_graph_->getVertexDegree(v);
    }
    auto avg_deg = (u_deg_sum + v_deg_sum) / (u_cand.size() + v_cand.size());
    check_edge_plan_cost = u_cand.size() * v_cand.size() * QuickLog2(avg_deg);

    if (true || aggregate_plan_cost <= check_edge_plan_cost) {
        flag_cnt_++;
        for (auto u1 : u_cand) {
            ui len;
            const ui* nbrs = data_graph_->getVertexNeighbors(u1, len);
            for (ui i = 0; i < len; ++i) {
                auto v1 = nbrs[i];
                if (query_graph_->getVertexLabel(v) != data_graph_->getVertexLabel(v1)) {
                    continue;
                }
                if (flag_[v1] != flag_cnt_) {
                    flag_[v1] = flag_cnt_;
                }
            }
        }
        int new_vcand_size = 0;
        for (auto v1 : v_cand) {
            if (flag_[v1] == flag_cnt_) {
                v_cand[new_vcand_size++] = v1;
            }
        }
        v_cand.resize(new_vcand_size);
    } else {
        int new_vcand_size = 0;
        for (auto& v : v_cand) {
            bool have_edge = false;
            for (auto& u : u_cand) {
                if (data_graph_->checkEdgeExistence(u, v)) {
                    have_edge = true;
                    break;
                }
            }

            if (have_edge) {
                v_cand[new_vcand_size++] = v;
            }
        }
        v_cand.resize(new_vcand_size);
    }
}

bool SubgraphFilter::Eliminate(bool& changed, std::vector<std::vector<unsigned>>& res) {
    auto n = query_graph_->getVerticesCount();
    std::vector<bool> vis(n, false);
    for (int i = 0; i < n; ++i) {
        int min_size = 1e9;
        int min_id = -1;
        for (int j = 0; j < n; ++j) {
            if (vis[j]) continue;
            if (res[j].size() < min_size) {
                min_size = res[j].size();
                min_id = j;
            }
        }
        auto u = min_id;
        vis[u] = true;
        ui len;
        auto vs = query_graph_->getVertexNeighbors(u, len);
        for (ui j = 0; j < len; ++j) {
            auto v = vs[j];
            auto ori_size = res[v].size();
            Prune(u, v, res);
            if (res[v].size() != ori_size) {
                changed = true;
            }
            if (res[v].empty()) {
                return false;
            }
        }
    }
    return true;
}

bool SubgraphFilter::Build(std::vector<std::vector<unsigned>>& res) {
    if (query_graph_->core_table_ == nullptr) {
        query_graph_->buildCoreTable();
    }
    std::unique_ptr<catalog> storage;
    storage = std::make_unique<catalog>(query_graph_, data_graph_);
    std::unique_ptr<preprocessor1> preprocessor(new preprocessor1());
    preprocessor->execute(query_graph_, data_graph_, storage.get(), true);

    std::unique_ptr<projection> projection_operator = std::make_unique<projection>(data_graph_->getVerticesCount() + 1);
    for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
        ui u = i;
        ui nbr_cnt;
        const ui* nbrs = query_graph_->getVertexNeighbors(u, nbr_cnt);
        for (ui j = 0; j < nbr_cnt; ++j) {
            ui v = nbrs[j];
            ui src = u;
            ui dst = v;
            ui kp = 0;
            if (src > dst) {
                std::swap(src, dst);
                kp = 1;
            }
            projection_operator->execute(&storage->edge_relations_[src][dst], kp, storage->candidate_sets_[u], storage->num_candidates_[u]);
            res.push_back(std::vector<ui>(storage->candidate_sets_[u], storage->candidate_sets_[u] + storage->num_candidates_[u]));
            if (res[i].empty()) return false;
            break;
        }
        if (nbr_cnt == 0) {
            ui len;
            auto vs = data_graph_->getVerticesByLabel(query_graph_->getVertexLabel(u), len);
            res.push_back(std::vector<ui>(vs, vs + len));
        }
    }
    assert(res.empty() == false);

    return true;
}

bool SubgraphFilter::Build(CandidateSpace& cs) {
    std::vector<std::vector<unsigned>> res;
    bool not_empty = Build(res);
    if (!not_empty) return false;
    cs.vcand_ = std::move(res);
    LinkEdge(cs);
    return true;
}

CandidateSpace SubgraphFilter::BuildFromCandidates(std::vector<std::vector<unsigned>>& candidates) {
    CandidateSpace cs;
    cs.vcand_ = candidates;
    return cs;
}

bool SubgraphFilter::FilterFromEdge(std::vector<std::vector<unsigned>>& res, unsigned qu, unsigned qv, unsigned du, unsigned dv) {
    uint32_t n = query_graph_->getVerticesCount();
    res.clear();
    res.resize(n);

    res[qu].push_back(du);
    res[qv].push_back(dv);

    std::vector<ui> order;

    // filter on triangle first
    for (int w = 0; w < query_graph_->getVerticesCount(); ++w) {
        if (w == qu || w == qv) continue;
        if (query_graph_->checkEdgeExistence(qu, w) == false) continue;
        if (query_graph_->checkEdgeExistence(qv, w) == false) continue;

        order.push_back(w);

        {
            ui len;
            auto nbr = data_graph_->getVertexNeighbors(du, len);
            flag_cnt_++;
            for (ui i = 0; i < len; ++i) {
                auto cand = nbr[i];
                if (data_graph_->getVertexLabel(cand) != query_graph_->getVertexLabel(w)) continue;
                if (flag_[cand] != flag_cnt_) {
                    flag_[cand] = flag_cnt_;
                }
            }
        }
        {
            flag_cnt_++;
            ui len;
            auto nbr = data_graph_->getVertexNeighbors(dv, len);
            for (ui i = 0; i < len; ++i) {
                auto cand = nbr[i];
                if (data_graph_->getVertexLabel(cand) != query_graph_->getVertexLabel(w)) continue;
                if (flag_[cand] == flag_cnt_) continue;
                if (flag_[cand] == flag_cnt_ - 1) {
                    flag_[cand] = flag_cnt_;
                    res[w].push_back(cand);
                }
            }
        }
        if (res[w].empty()) {
            return false;
        }
    }

    // generate initial candidates for other query vertices
    for (;;) {
        // find the query vertex with the maximum number of filtered neighbors
        int max_bn = -1;
        int max_id = -1;
        for (int j = 0; j < n; ++j) {
            if (res[j].empty() == false) continue;
            int bn = 0;
            ui nbr_n;
            const ui* nbrs = query_graph_->getVertexNeighbors(j, nbr_n);
            for (ui k = 0; k < nbr_n; ++k) {
                auto nbr = nbrs[k];
                if (res[nbr].empty() == false) bn++;
            }
            if (bn > max_bn) {
                max_bn = bn;
                max_id = j;
            }
        }
        // assert(max_id != -1);
        if (max_id == -1) break;

        order.push_back(max_id);

        // std::cout << "max_id: " << max_id << ", max_bn: " << max_bn << "\n";
        // std::cout << "res: " << res << "\n";

        // intersect neighbors of candidates of u's neighbors
        auto u = max_id;

        res[u].reserve(100);

        ui nbr_n;
        const ui* nbrs = query_graph_->getVertexNeighbors(u, nbr_n);

        int filtered_neighbors = 0;
        for (ui k = 0; k < nbr_n; ++k) {
            auto nbr = nbrs[k];
            if (res[nbr].empty() == false) filtered_neighbors++;
        }
        bool first = true;
        for (ui k = 0; k < nbr_n; ++k) {
            auto nbr = nbrs[k];
            if (res[nbr].empty()) continue;

            filtered_neighbors--;
            flag_cnt_++;
            for (auto v : res[nbr]) {
                ui v_nbr_n;
                const ui* v_nbrs = data_graph_->getVertexNeighbors(v, v_nbr_n);

                for (int j = 0; j < v_nbr_n; ++j) {
                    auto cand = v_nbrs[j];
                    if (data_graph_->getVertexLabel(cand) != query_graph_->getVertexLabel(u)) continue;
                    if (flag_[cand] == flag_cnt_) continue;
                    if (first || flag_[cand] == flag_cnt_ - 1) {
                        flag_[cand] = flag_cnt_;
                        if (filtered_neighbors == 0) {
                            res[u].push_back(cand);
                        }
                    }
                }
            }

            first = false;
        }
        if (res[u].empty()) {
            // std::cout << "No candidates for query vertex " << u << "\n";
            return false;
        }
        std::sort(res[u].begin(), res[u].end());
    }

    for (int u = 0; u < n; ++u) {
        auto u_nlf = query_graph_->getVertexNLF(u);
        int new_res_size = 0;
        for (int i = 0; i < res[u].size(); ++i) {
            auto v = res[u][i];
            auto v_nlf = data_graph_->getVertexNLF(v);
            if (v_nlf->size() >= u_nlf->size()) {
                if (nlf_filter::check(u_nlf, v_nlf)) {
                    res[u][new_res_size++] = v;
                }
            }
        }
        if (new_res_size == 0) {
            return false;
        }
        res[u].resize(new_res_size);
    }

    for (int i = order.size() - 1; i >= 0; --i) {
        auto u = order[i];
        for (int j = i + 1; j < order.size(); ++j) {
            auto v = order[j];
            if (query_graph_->checkEdgeExistence(u, v) == false) continue;
            Prune(v, u, res);
            if (res[u].empty()) {
                // std::cout << "No candidates for query vertex " << v << " after pruning\n";
                return false;
            }
        }
    }

    return true;
}

bool SubgraphFilter::FilterFromEdgeBatch(std::vector<std::vector<unsigned>>& res,
                                         unsigned qu, unsigned qv, std::vector<std::pair<unsigned, unsigned>>& edge_list) {
    uint32_t n = query_graph_->getVerticesCount();
    res.clear();
    res.resize(n);

    res[qu].reserve(edge_list.size());
    res[qv].reserve(edge_list.size());
    for (auto [du, dv] : edge_list) {
        res[qu].push_back(du);
        res[qv].push_back(dv);
    }
    std::sort(res[qu].begin(), res[qu].end());
    res[qu].erase(std::unique(res[qu].begin(), res[qu].end()), res[qu].end());
    std::sort(res[qv].begin(), res[qv].end());
    res[qv].erase(std::unique(res[qv].begin(), res[qv].end()), res[qv].end());

    // std::cout << "Initial candidates for " << qu << ": " << res[qu] << "\n";
    // std::cout << "Initial candidates for " << qv << ": " << res[qv] << "\n";

    std::vector<ui> order;

    // get triangle neighbors
    std::vector<ui> w_list;
    for (int w = 0; w < query_graph_->getVerticesCount(); ++w) {
        if (w == qu || w == qv) continue;
        if (query_graph_->checkEdgeExistence(qu, w) == false) continue;
        if (query_graph_->checkEdgeExistence(qv, w) == false) continue;
        w_list.push_back(w);
        order.push_back(w);
    }

    // filter on triangle first
    bool have_answer = false;
    for (auto [du, dv] : edge_list) {
        std::vector<std::vector<ui>> tmp_res;
        tmp_res.resize(w_list.size());
        bool edge_have_answer = true;
        for (int i = 0; i < w_list.size(); ++i) {
            auto w = w_list[i];

            ui len;
            auto nbr = data_graph_->getVertexNeighbors(du, len);
            flag_cnt_++;
            for (ui j = 0; j < len; ++j) {
                auto cand = nbr[j];
                if (data_graph_->getVertexLabel(cand) != query_graph_->getVertexLabel(w)) continue;
                if (flag_[cand] != flag_cnt_) {
                    flag_[cand] = flag_cnt_;
                }
            }

            flag_cnt_++;
            nbr = data_graph_->getVertexNeighbors(dv, len);
            for (ui j = 0; j < len; ++j) {
                auto cand = nbr[j];
                if (data_graph_->getVertexLabel(cand) != query_graph_->getVertexLabel(w)) continue;
                if (flag_[cand] == flag_cnt_) continue;
                if (flag_[cand] == flag_cnt_ - 1) {
                    flag_[cand] = flag_cnt_;
                    tmp_res[i].push_back(cand);
                }
            }

            if (tmp_res[i].empty()) {
                edge_have_answer = false;
                break;
            }
        }

        if (edge_have_answer) {
            have_answer = true;
            for (int i = 0; i < w_list.size(); ++i) {
                auto w = w_list[i];
                std::vector<ui> merged;
                MergeTwoSortedLists(res[w], tmp_res[i], merged);
                res[w] = std::move(merged);
            }
        }
    }

    if (!have_answer) {
        // std::cout << "No candidates for triangle neighbors\n";
        return false;
    }

    // generate initial candidates for other query vertices
    for (;;) {
        // find the query vertex with the maximum number of filtered neighbors
        int max_bn = -1;
        int max_id = -1;
        for (int j = 0; j < n; ++j) {
            if (res[j].empty() == false) continue;
            int bn = 0;
            ui nbr_n;
            const ui* nbrs = query_graph_->getVertexNeighbors(j, nbr_n);
            for (ui k = 0; k < nbr_n; ++k) {
                auto nbr = nbrs[k];
                if (res[nbr].empty() == false) bn++;
            }
            if (bn > max_bn) {
                max_bn = bn;
                max_id = j;
            }
        }
        // assert(max_id != -1);
        if (max_id == -1) break;

        order.push_back(max_id);

        // std::cout << "max_id: " << max_id << ", max_bn: " << max_bn << "\n";
        // std::cout << "res: " << res << "\n";

        // intersect neighbors of candidates of u's neighbors
        auto u = max_id;

        res[u].reserve(100);

        ui nbr_n;
        const ui* nbrs = query_graph_->getVertexNeighbors(u, nbr_n);

        int filtered_neighbors = 0;
        for (ui k = 0; k < nbr_n; ++k) {
            auto nbr = nbrs[k];
            if (res[nbr].empty() == false) filtered_neighbors++;
        }
        bool first = true;
        for (ui k = 0; k < nbr_n; ++k) {
            auto nbr = nbrs[k];
            if (res[nbr].empty()) continue;

            filtered_neighbors--;
            flag_cnt_++;
            for (auto v : res[nbr]) {
                ui v_nbr_n;
                const ui* v_nbrs = data_graph_->getVertexNeighbors(v, v_nbr_n);

                for (int j = 0; j < v_nbr_n; ++j) {
                    auto cand = v_nbrs[j];
                    if (data_graph_->getVertexLabel(cand) != query_graph_->getVertexLabel(u)) continue;
                    if (flag_[cand] == flag_cnt_) continue;
                    if (first || flag_[cand] == flag_cnt_ - 1) {
                        flag_[cand] = flag_cnt_;
                        if (filtered_neighbors == 0) {
                            res[u].push_back(cand);
                        }
                    }
                }
            }

            first = false;
        }
        if (res[u].empty()) {
            // std::cout << "No candidates for query vertex " << u << "\n";
            return false;
        }
        std::sort(res[u].begin(), res[u].end());
    }

    for (int u = 0; u < n; ++u) {
        auto u_nlf = query_graph_->getVertexNLF(u);
        int new_res_size = 0;
        for (int i = 0; i < res[u].size(); ++i) {
            auto v = res[u][i];
            auto v_nlf = data_graph_->getVertexNLF(v);
            if (v_nlf->size() >= u_nlf->size()) {
                if (nlf_filter::check(u_nlf, v_nlf)) {
                    res[u][new_res_size++] = v;
                }
            }
        }
        if (new_res_size == 0) {
            return false;
        }
        res[u].resize(new_res_size);
    }

    for (int i = order.size() - 1; i >= 0; --i) {
        auto u = order[i];
        for (int j = i + 1; j < order.size(); ++j) {
            auto v = order[j];
            if (query_graph_->checkEdgeExistence(u, v) == false) continue;
            Prune(v, u, res);
            if (res[u].empty()) {
                // std::cout << "No candidates for query vertex " << v << " after pruning\n";
                return false;
            }
        }
    }

    return true;
}

void SubgraphFilter::LinkEdge(CandidateSpace& cs) {
    cs.ecand_.clear();
    cs.ecand_.resize(query_graph_->getVerticesCount());
    for (ui i = 0; i < query_graph_->getVerticesCount(); ++i) {
        cs.ecand_[i].resize(query_graph_->getVerticesCount());
    }
    // for (ui u = 0; u < query_graph_->getVerticesCount(); ++u) {
    //     ui len;
    //     const ui* neighbors = query_graph_->getVertexNeighbors(u, len);
    //     for (ui i = 0; i < len; ++i) {
    //         ui v = neighbors[i];
    //         for (auto x : cs.vcand_[u]) {
    //             ui y_num;
    //             const ui* ys = data_graph_->getVertexNeighbors(x, y_num);
    //             std::vector<ui> y_vec(ys, ys + y_num);
    //             IntersectSortedLists(y_vec, cs.vcand_[v]);
    //             cs.ecand_[u][v].push_back(std::move(y_vec));
    //         }
    //     }
    // }
    for (ui qv = 0; qv < query_graph_->getVerticesCount(); ++qv) {
        ui len;
        const ui* neighbors = query_graph_->getVertexNeighbors(qv, len);
        flag_cnt_++;
        for (auto dv : cs.vcand_[qv]) {
            flag_[dv] = flag_cnt_;
        }
        for (ui i = 0; i < len; ++i) {
            ui qu = neighbors[i];
            cs.ecand_[qu][qv].resize(cs.vcand_[qu].size());
            for (int du_idx = 0; du_idx < cs.vcand_[qu].size(); ++du_idx) {
                ui du = cs.vcand_[qu][du_idx];
                ui du_nbr_num;
                const ui* du_nbrs = data_graph_->getVertexNeighbors(du, du_nbr_num);
                cs.ecand_[qu][qv][du_idx].reserve(du_nbr_num);
                for (ui j = 0; j < du_nbr_num; ++j) {
                    auto dv = du_nbrs[j];
                    if (flag_[dv] == flag_cnt_) {
                        cs.ecand_[qu][qv][du_idx].push_back(dv);
                    }
                }
            }
        }
    }
}

void SubgraphFilter::Merge(CandidateSpace& src, CandidateSpace& dst, Graph* query_graph) {
    assert(src.vcand_.size() == dst.vcand_.size());

    CandidateSpace merged_cs;
    merged_cs.vcand_.clear();
    merged_cs.vcand_.resize(src.vcand_.size());
    merged_cs.ecand_.clear();
    merged_cs.ecand_.resize(src.vcand_.size());
    for (int i = 0; i < src.vcand_.size(); ++i) {
        merged_cs.ecand_[i].resize(src.vcand_.size());
    }

    for (int u = 0; u < src.vcand_.size(); ++u) {
        merged_cs.vcand_[u].reserve(src.vcand_[u].size() + dst.vcand_[u].size());
        merged_cs.ecand_[u].reserve(src.ecand_[u].size() + dst.ecand_[u].size());

        int idx0 = 0, idx1 = 0;
        while (idx0 < src.vcand_[u].size() && idx1 < dst.vcand_[u].size()) {
            if (src.vcand_[u][idx0] == dst.vcand_[u][idx1]) {
                // update vcand_
                merged_cs.vcand_[u].push_back(src.vcand_[u][idx0]);
                // update ecand_
                ui len;
                auto nbrs = query_graph->getVertexNeighbors(u, len);
                for (int j = 0; j < len; ++j) {
                    auto v = nbrs[j];
                    std::vector<ui> merged_edge_cands;
                    merged_edge_cands.reserve(src.ecand_[u][v][idx0].size() + dst.ecand_[u][v][idx1].size());
                    MergeTwoSortedLists(
                        src.ecand_[u][v][idx0],
                        dst.ecand_[u][v][idx1],
                        merged_edge_cands);
                    merged_cs.ecand_[u][v].push_back(std::move(merged_edge_cands));
                }

                idx0++;
                idx1++;
            } else if (src.vcand_[u][idx0] < dst.vcand_[u][idx1]) {
                // update vcand_
                merged_cs.vcand_[u].push_back(src.vcand_[u][idx0]);
                // update ecand_
                ui len;
                auto nbrs = query_graph->getVertexNeighbors(u, len);
                for (int j = 0; j < len; ++j) {
                    auto v = nbrs[j];
                    merged_cs.ecand_[u][v].push_back(std::move(src.ecand_[u][v][idx0]));
                }
                idx0++;
            } else {
                // update vcand_
                merged_cs.vcand_[u].push_back(dst.vcand_[u][idx1]);
                // update ecand_
                ui len;
                auto nbrs = query_graph->getVertexNeighbors(u, len);
                for (int j = 0; j < len; ++j) {
                    auto v = nbrs[j];
                    merged_cs.ecand_[u][v].push_back(std::move(dst.ecand_[u][v][idx1]));
                }
                idx1++;
            }
        }

        while (idx0 < src.vcand_[u].size()) {
            // update vcand_
            merged_cs.vcand_[u].push_back(src.vcand_[u][idx0]);
            // update ecand_
            ui len;
            auto nbrs = query_graph->getVertexNeighbors(u, len);
            for (int j = 0; j < len; ++j) {
                auto v = nbrs[j];
                merged_cs.ecand_[u][v].push_back(std::move(src.ecand_[u][v][idx0]));
            }
            idx0++;
        }

        while (idx1 < dst.vcand_[u].size()) {
            // update vcand_
            merged_cs.vcand_[u].push_back(dst.vcand_[u][idx1]);
            // update ecand_
            ui len;
            auto nbrs = query_graph->getVertexNeighbors(u, len);
            for (int j = 0; j < len; ++j) {
                auto v = nbrs[j];
                merged_cs.ecand_[u][v].push_back(std::move(dst.ecand_[u][v][idx1]));
            }
            idx1++;
        }
    }
    dst = std::move(merged_cs);
}

void SubgraphFilter::Remove(CandidateSpace& src, Graph* query_graph, ui qu, ui qv, ui du, ui dv) {
    std::vector<std::vector<ui>> removed;
    std::vector<ui> remove_cnt;
    std::queue<std::pair<ui, ui>> q;
    removed.resize(query_graph->getVerticesCount());
    for (int i = 0; i < query_graph->getVerticesCount(); ++i) {
        removed[i].resize(src.vcand_[i].size(), 0);
    }
    remove_cnt.resize(query_graph->getVerticesCount(), 0);

    int du_pos = std::lower_bound(src.vcand_[qu].begin(), src.vcand_[qu].end(), du) - src.vcand_[qu].begin();
    if (du_pos < src.vcand_[qu].size() && src.vcand_[qu][du_pos] == du) {
        auto& link = src.ecand_[qu][qv][du_pos];
        if (std::binary_search(link.begin(), link.end(), dv)) {
            EraseValueFromSortedVector(link, dv);
        }
        if (link.empty()) {
            q.push({qu, du_pos});
            removed[qu][du_pos] = 1;
            remove_cnt[qu]++;
        }
    }

    int dv_pos = std::lower_bound(src.vcand_[qv].begin(), src.vcand_[qv].end(), dv) - src.vcand_[qv].begin();
    if (dv_pos < src.vcand_[qv].size() && src.vcand_[qv][dv_pos] == dv) {
        auto & link = src.ecand_[qv][qu][dv_pos];
        if (std::binary_search(link.begin(), link.end(), du)) {
            EraseValueFromSortedVector(link, du);
        }
        if (link.empty()) {
            q.push({qv, dv_pos});
            removed[qv][dv_pos] = 1;
            remove_cnt[qv]++;
        }
    }

    while (!q.empty()) {
        auto [qu, du_pos] = q.front();
        auto du = src.vcand_[qu][du_pos];
        q.pop();
        ui len;
        auto nbrs = query_graph->getVertexNeighbors(qu, len);
        for (int i = 0; i < len; ++i) {
            auto qv = nbrs[i];
            for (int j = 0; j < src.ecand_[qu][qv][du_pos].size(); ++j) {
                auto dv = src.ecand_[qu][qv][du_pos][j];
                auto dv_pos = std::lower_bound(src.vcand_[qv].begin(), src.vcand_[qv].end(), dv) - src.vcand_[qv].begin();
                if (removed[qv][dv_pos]) continue;
                if (std::binary_search(src.ecand_[qv][qu][dv_pos].begin(), src.ecand_[qv][qu][dv_pos].end(), du)) {
                    EraseValueFromSortedVector(src.ecand_[qv][qu][dv_pos], du);
                }
                if (src.ecand_[qv][qu][dv_pos].empty()) {
                    q.push({qv, dv_pos});
                    removed[qv][dv_pos] = 1;
                    remove_cnt[qv]++;
                }
            }
        }
    }

    // need to be optimized: count the number of candidate deletions of query node before removal
    for (int u = 0; u < query_graph->getVerticesCount(); ++u) {
        if (remove_cnt[u] == 0) continue;
        // update src.vcand_[u]
        std::vector<ui> new_vcand;
        for (int i = 0; i < src.vcand_[u].size(); ++i) {
            if (removed[u][i] == 0) {
                new_vcand.push_back(src.vcand_[u][i]);
            }
        }
        src.vcand_[u] = std::move(new_vcand);
        // update src.ecand_[u][v]
        ui len;
        auto nbrs = query_graph->getVertexNeighbors(u, len);
        for (int i = 0; i < len; ++i) {
            auto v = nbrs[i];
            std::vector<std::vector<ui>> new_ecand_uv;
            for (int j = 0; j < src.ecand_[u][v].size(); ++j) {
                if (removed[u][j] == 0) {
                    new_ecand_uv.push_back(src.ecand_[u][v][j]);
                }
            }
            src.ecand_[u][v] = std::move(new_ecand_uv);
        }
    }
}