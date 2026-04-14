#include "tree_partitioner.h"

#include <cassert>
#include <set>

#include "MyUtils.h"
#include "finergrained_decomposer.h"
#include "global_vars.h"
#include "pretty_print.h"

TreePartitioner::TreePartitioner(SubgraphEstimator *subgraph_estimator, Graph *query_graph, bool need_connected)
    : estimator_(subgraph_estimator), query_graph_(query_graph), need_connected_(need_connected) {}

void TreePartitioner::GetDfsOrder(std::vector<ui> &dfs_seq, std::vector<ui> &bcc) {
    auto u = dfs_seq.back();
    ui len;
    auto nbrs = query_graph_->getVertexNeighbors(u, len);
    for (ui i = 0; i < len; ++i) {
        auto v = nbrs[i];
        if (std::find(bcc.begin(), bcc.end(), v) == bcc.end()) continue;  // not in the same bcc
        if (std::find(dfs_seq.begin(), dfs_seq.end(), v) != dfs_seq.end()) continue;  // visited
        dfs_seq.push_back(v);
        GetDfsOrder(dfs_seq, bcc);
    }
}

long double TreePartitioner::EstimateStateEmbeddingNum(std::vector<ui> &state) {
    if (estimator_ == nullptr) return 0.0;
    long double ret = 0;
    for (auto &part : state) ret += estimator_->Estimate(part);
    return ret;
}

bool TreePartitioner::CheckFutureConnectivity1(std::vector<ui>& state, ui cur, const std::vector<ui>& dfs_seq) {
    bool ret = true;
    for (auto &part: state) {
        int cnt = 0;
        int part_size = __builtin_popcount(part);
        for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
            if (!(part & (1 << i))) continue;
            auto u_no = dfs_no_[i];
            for (int j = i + 1; j < query_graph_->getVerticesCount(); ++j) {
                if (!(part & (1 << j))) continue;
                auto v_no = dfs_no_[j];
                if (floyd_[cur + 1][u_no][v_no]) {
                    auto fu = Find(i);
                    auto fv = Find(j);
                    if (fu == fv) continue;
                    UnionFind_[fu] = fv;
                    cnt++;
                    if (cnt == part_size - 1) break;
                }
            }
            if (cnt == part_size - 1) break;
        }
        if (cnt < part_size - 1) {
            ret = false;
            break;
        }
    }
    
    for (auto u: dfs_seq) UnionFind_[u] = u;

    return ret;
}


void TreePartitioner::TreePartitionSearch(std::vector<ui> &cur_state, std::vector<ui> &edge_unions, ui cur, const std::vector<ui> &dfs_seq,
                                             ui lim_width, std::vector<std::vector<ui>> &next_states, std::vector<ui> &answer, long double &answer_score) {
    state_cnt_++;
    if (cur == dfs_seq.size()) {
        long double new_score = EstimateStateEmbeddingNum(cur_state);
        if (answer.empty() || new_score < answer_score) {
            answer = cur_state;
            answer_score = new_score;
        }
        return;
    }

    auto u = dfs_seq[cur];
    ui edge_union = 0;
    ui len;
    auto nbrs = query_graph_->getVertexNeighbors(u, len);
    for (ui i = 0; i < len; ++i) edge_union |= (1 << nbrs[i]);

    // extend the state by adding the new node u as a new part
    int connect_cnt = 0;
    for (auto part: cur_state) {
        if (edge_union & part) {
            connect_cnt++;
            if (connect_cnt > 1) break;  
        }
    }
    if (connect_cnt == 1) {
        cur_state.push_back(1 << u);
        edge_unions.push_back(edge_union);
        if (need_connected_ == false || CheckFutureConnectivity1(cur_state, cur, dfs_seq)) {
            TreePartitionSearch(cur_state, edge_unions, cur + 1, dfs_seq, lim_width, next_states, answer, answer_score);
        }
        cur_state.pop_back();
        edge_unions.pop_back();
        if (!answer.empty() && estimator_ == nullptr) return;
    }

    // extend the state by adding the new node u to existing parts
    for (ui part_id = 0; part_id < cur_state.size(); ++part_id) {
        bool is_super_tree = true;
        auto part_edge_union = edge_unions[part_id];
        int idx = 0;
        // check if new edges occurs after adding the new node
        for (int other_part_id = 0; other_part_id < cur_state.size(); ++other_part_id) {
            if (other_part_id == part_id) continue;
            auto &other_part = cur_state[other_part_id];
            if ((part_edge_union & other_part) == 0 && (edge_union & other_part) != 0) {
                is_super_tree = false;
                break;
            }
        }
        if (is_super_tree) {
            cur_state[part_id] |= (1 << u);
            if ((need_connected_ == false || CheckFutureConnectivity1(cur_state, cur, dfs_seq))) {
                if (__builtin_popcount(cur_state[part_id]) > lim_width) {
                    next_states.push_back(cur_state);
                } else {
                    auto ori_edge_union = edge_unions[part_id];
                    edge_unions[part_id] |= edge_union;
                    TreePartitionSearch(cur_state, edge_unions, cur + 1, dfs_seq, lim_width, next_states, answer, answer_score);
                    edge_unions[part_id] = ori_edge_union;
                }
            }
            cur_state[part_id] &= ~(1 << u);
        }
        if (!answer.empty() && (estimator_ == nullptr || need_connected_ == false)) return;
    }
}

void TreePartitioner::TreePartition(std::vector<ui> &dfs_seq, std::vector<ui> &ret_state, long double &ret_score) {
    memset(dfs_no_.data(), -1, sizeof(int) * dfs_no_.size());
    for (int i = 0; i < dfs_seq.size(); ++i) {
        dfs_no_[dfs_seq[i]] = i;
    }

    floyd_.resize(dfs_seq.size() + 1);
    for (int k = 0; k <= dfs_seq.size(); ++k) {
        floyd_[k].resize(dfs_seq.size());
        for (int i = 0; i < dfs_seq.size(); ++i) {
            floyd_[k][i].resize(dfs_seq.size());
            floyd_[k][i].assign(dfs_seq.size(), false);
        }
    }
    for (auto u : dfs_seq) {
        ui n;
        const ui *nbrs = query_graph_->getVertexNeighbors(u, n);
        auto u_no = dfs_no_[u];
        for (int i = 0; i < n; ++i) {
            auto v = nbrs[i];
            auto v_no = dfs_no_[v];
            if (v_no == -1) continue;
            floyd_[dfs_seq.size()][u_no][v_no] = floyd_[dfs_seq.size()][v_no][u_no] = true;
        }
    }
    for (int k = dfs_seq.size() - 1; k >= 0; --k) {
        for (int i = 0; i < dfs_seq.size(); ++i) {
            for (int j = 0; j < dfs_seq.size(); ++j) {
                floyd_[k][i][j] = floyd_[k + 1][i][j] || (floyd_[k + 1][i][k] && floyd_[k + 1][k][j]);
            }
        }
    }

    std::vector<std::vector<ui>> states, next_states;
    std::vector<ui> state;
    state.push_back(1 << dfs_seq[0]);
    states.push_back(state);
    state_cnt_ = 0;
    std::vector<ui> answer;
    long double answer_score = 1e18;
    for (ui lim_width = 1; lim_width <= dfs_seq.size(); ++lim_width) {
        next_states.clear();
        for (auto &state : states) {
            ui cur = 0;
            for (auto &part : state) cur += __builtin_popcount(part);
            
            state.reserve(dfs_seq.size());

            std::vector<ui> edge_unions;
            for (auto part: state) {
                auto edge_union = 0;
                for (int u = 0; u < query_graph_->getVerticesCount(); ++u) {
                    if ((part & (1 << u)) == 0) continue;  
                    ui len;
                    auto nbrs = query_graph_->getVertexNeighbors(u, len);
                    for (int i = 0; i < len; ++i) edge_union |= (1 << nbrs[i]);
                }
                edge_unions.push_back(edge_union);
            }

            TreePartitionSearch(state, edge_unions, cur, dfs_seq, lim_width, next_states, answer, answer_score);

            if (!answer.empty()) {
                ret_state = answer;
                ret_score = answer_score;
                return;
            }
        }
        states = next_states;
    }
}

void TreePartitioner::PartitionOnBCC(std::vector<ui> &bcc, std::vector<ui> &decomp) {
    ui SCORE_LIMIT = MAX_EMBEDDING_NUM;
    std::vector<ui> dfs_seq;
    dfs_seq.push_back(bcc[0]);
    GetDfsOrder(dfs_seq, bcc);

    long double answer_score;
    TreePartition(dfs_seq, decomp, answer_score);
    // std::cout << "origin partitioning: " << decomp << std::endl;
    // std::cout << "lossless answer score: " << answer_score << std::endl;

    // partition further
    if (estimator_ != nullptr) {
        // std::cout << "further partitioning..." << std::endl;
        // std::cout << "origin partitioning: " << decomp << std::endl;
        if (need_connected_) {
            for (;;) {
                int exceed_limit_part_id = -1;
                long double est_val;
                for (ui i = 0; i < decomp.size(); ++i) {
                    if (__builtin_popcount(decomp[i]) == 1) continue;  // skip single node parts
                    est_val = estimator_->Estimate(decomp[i]);
                    if (est_val > SCORE_LIMIT) {
                        exceed_limit_part_id = i;
                        break;
                    }
                }
                if (exceed_limit_part_id == -1) break;

                is_tree_ = false;

                auto exceed_limit_part = decomp[exceed_limit_part_id];

                std::cout << "old part:";
                for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
                    if (exceed_limit_part & (1 << i)) std::cout << " " << i;
                }
                std::cout << std::endl;
                std::cout << "estimate value: " << est_val << std::endl;

                decomp.erase(decomp.begin() + exceed_limit_part_id);

                std::unique_ptr<FinergrainedDecomposer1> fdc(new FinergrainedDecomposer1(query_graph_, estimator_));
                std::vector<ui> new_parts;
                fdc->Exec(exceed_limit_part, decomp, SCORE_LIMIT, new_parts);

                std::cout << "new parts: ";
                for (auto &part : new_parts) {
                    for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
                        if (part & (1 << i)) std::cout << " " << i;
                    }
                    std::cout << " | ";
                }
                std::cout << std::endl;

                decomp.insert(decomp.end(), new_parts.begin(), new_parts.end());
            }
        }
        else {
            std::vector<ui> new_parts;
            for (auto part: decomp) {
                ui part1 = 0;
                for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
                    if (part & (1 << i)) {
                        auto part2 = part1 | (1 << i);
                        if (estimator_->Estimate(part2) > SCORE_LIMIT) {
                            assert(part1 != 0);
                            new_parts.push_back(part1);
                            part1 = 1 << i;  // start a new part
                        } else {
                            part1 = part2;
                        }
                    }
                }
                assert(part1 != 0);
                new_parts.push_back(part1);
            }
            decomp = new_parts;
        }
        // std::cout << "finished further partitioning, new parts: " << decomp << std::endl;
    }
}


void TreePartitioner::GetBccs(int u, int fr, int &cnt, std::vector<int> &dfn, std::vector<int> &low, std::vector<int> &stk, std::vector<std::vector<ui>> &bccs) {
    dfn[u] = low[u] = ++cnt;
    stk.push_back(u);
    ui len;
    const ui *nbrs = query_graph_->getVertexNeighbors(u, len);
    for (int i = 0; i < len; ++i) {
        auto v = nbrs[i];
        if (v == fr) continue;
        if (dfn[v] == 0) {
            GetBccs(v, u, cnt, dfn, low, stk, bccs);
            low[u] = std::min(low[u], low[v]);
        } else {
            low[u] = std::min(low[u], dfn[v]);
        }
    }
    if (dfn[u] == low[u]) {
        bccs.push_back(std::vector<ui>());
        while (stk.back() != u) {
            bccs.back().push_back(stk.back());
            stk.pop_back();
        }
        bccs.back().push_back(stk.back());
        stk.pop_back();
    }
}

void TreePartitioner::QueryDecomposition(std::vector<std::vector<ui>> &res) {
    UnionFind_.resize(query_graph_->getVerticesCount());
    for (int i = 0; i < query_graph_->getVerticesCount(); ++i) UnionFind_[i] = i;
    all_nodes_ = new ui[query_graph_->getVerticesCount()];
    dfs_no_.resize(query_graph_->getVerticesCount(), -1);

    std::vector<std::vector<ui>> bccs1;
    std::vector<int> dfn(query_graph_->getVerticesCount(), 0), low(query_graph_->getVerticesCount(), 0), stk;
    int dfs_cnt = 0;
    for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
        if (dfn[i] == 0) GetBccs(i, -1, dfs_cnt, dfn, low, stk, bccs1);
    }

    // for (auto &bcc: bccs1) {
    //   std::cout << bcc << std::endl;
    // }

    for (auto &bcc : bccs1) {
        std::vector<ui> decomp;
        std::sort(bcc.begin(), bcc.end());
        PartitionOnBCC(bcc, decomp);
        for (auto &vec : decomp) {
            res.emplace_back();
            for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
                if (vec & (1 << i)) res.back().push_back(i);
            }
        }
    }
}
