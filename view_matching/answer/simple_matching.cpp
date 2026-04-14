#include "simple_matching.h"

#include <chrono>
#include <queue>

#include "MyUtils.h"
#include "pretty_print.h"
#include "utility/primitive/nlf_filter.h"

void SimpleMatcher::GetOrder() {
    q_->buildCoreTable();
    std::vector<bool> visited(q_->getVerticesCount(), false);
    // find the core with max degree and smallest candidate set
    int s = -1;
    ui max_deg = 0;
    ui min_can_size = 1e9;
    for (int i = 0; i < q_->getVerticesCount(); ++i) {
        auto core_value = q_->getCoreValue(i);
        if (q_->getCoreValue(i) < 2) continue;
        auto deg = q_->getVertexDegree(i);
        if (deg > max_deg || (deg == max_deg && cand_[i].size() < min_can_size)) {
            max_deg = deg;
            min_can_size = cand_[i].size();
            s = i;
        }
    }
    if (s == -1) {
        ui max_deg = 0;
        ui min_can_size = 1e9;
        for (int i = 0; i < q_->getVerticesCount(); ++i) {
            auto core_value = q_->getCoreValue(i);
            auto deg = q_->getVertexDegree(i);
            if (deg > max_deg || (deg == max_deg && cand_[i].size() < min_can_size)) {
                max_deg = deg;
                min_can_size = cand_[i].size();
                s = i;
            }
        }
    }
    visited[s] = true;
    order_.push_back(s);
    for (int i = 1; i < q_->getVerticesCount(); ++i) {
        std::vector<ui> frontiers;
        std::vector<bool> in_frontiers(q_->getVerticesCount(), false);
        for (auto u : order_) {
            ui len;
            auto nbrs = q_->getVertexNeighbors(u, len);
            for (int j = 0; j < len; ++j) {
                if (!visited[nbrs[j]] && !in_frontiers[nbrs[j]]) {
                    frontiers.push_back(nbrs[j]);
                    in_frontiers[nbrs[j]] = true;
                }
            }
        }
        int max_back_nbr_cnt = 0;
        int min_can_size = 1e9;
        int next = -1;
        for (auto u : frontiers) {
            int back_nbr_cnt = 0;
            for (auto v : order_) {
                if (q_->checkEdgeExistence(u, v)) {
                    back_nbr_cnt++;
                }
            }
            if (back_nbr_cnt > max_back_nbr_cnt || (back_nbr_cnt == max_back_nbr_cnt && cand_[u].size() < min_can_size)) {
                max_back_nbr_cnt = back_nbr_cnt;
                min_can_size = cand_[u].size();
                next = u;
            }
        }
        order_.push_back(next);
        visited[next] = true;
    }
}

void SimpleMatcher::GetLocalCandidate(int cur, std::vector<int>& embedding,
                                      std::vector<ui>& local_candidate) {
    std::vector<int> bn_positions = back_nbr_poss_[cur];
    auto u = order_[cur];
    bool first = true;
    for (auto bn_position : bn_positions) {
        auto v = embedding[bn_position];
        auto bn = order_[bn_position];
        auto v_pos = std::lower_bound(cand_[bn].begin(), cand_[bn].end(), v) - cand_[bn].begin();
        if (v_pos == cand_[bn].size() || cand_[bn][v_pos] != v) {
            local_candidate.clear();
            return;
        }

        ui len;
        auto nbrs = g_->getVertexNeighbors(v, len);
        std::vector<ui> v_nbrs(nbrs, nbrs + len);

        if (first) {
            local_candidate = v_nbrs;
            first = false;
        } else {
            // IntersectSortedLists(local_candidate, cs_.ecand_[bn][u][v_pos]);
            IntersectSortedLists(local_candidate, v_nbrs);
        }
    }
    if (first) {
        local_candidate = cand_[u];
    }
    else {
        IntersectSortedLists(local_candidate, cand_[u]);
    }
}

void SimpleMatcher::Dfs(int cur, std::vector<int>& embedding, std::vector<std::vector<int>>& ret_embeddings) {
    state_cnt_++;
    if (state_cnt_ >= state_limit_) return;
    if (cur == q_->getVerticesCount()) {
        ret_embeddings.push_back(embedding);
        return;
    }
    auto u = order_[cur];
    if (cand_[u].size() == 1) {
        auto v = cand_[u][0];
        if (vis_[v]) return;
        auto& bn = back_nbr_poss_[cur];
        for (auto u1 : bn) {
            auto v1 = embedding[u1];
            if (!g_->checkEdgeExistence(v, v1)) return;
        }
        vis_[v] = true;
        embedding.push_back(v);
        Dfs(cur + 1, embedding, ret_embeddings);
        embedding.pop_back();
        vis_[v] = false;
    } else {
        std::vector<ui> lc;
        GetLocalCandidate(cur, embedding, lc);
        for (auto v : lc) {
            if (vis_[v]) continue;
            vis_[v] = true;
            embedding.push_back(v);
            Dfs(cur + 1, embedding, ret_embeddings);
            embedding.pop_back();
            vis_[v] = false;
            if (ret_embeddings.size() >= embedding_limit_) return;
            if (state_cnt_ >= state_limit_) return;
        }
    }
}

void SimpleMatcher::Filter() {
    cand_.resize(q_->getVerticesCount());
    for (int u = 0; u < q_->getVerticesCount(); ++u) {
        ui len;
        auto vs = g_->getVerticesByLabel(q_->getVertexLabel(u), len);

        cand_[u].reserve(len);

        auto u_nlf = q_->getVertexNLF(u);

        for (int j = 0; j < len; ++j) {
            auto v = vs[j];
            auto v_nlf = g_->getVertexNLF(v);
            if (v_nlf->size() >= u_nlf->size()) {
                if (nlf_filter::check(u_nlf, v_nlf)) {
                    cand_[u].push_back(v);
                }
            }
        }

        if (cand_[u].empty()) {
            cand_.clear();
            break;
        }
    }
}

void SimpleMatcher::Match(std::vector<std::vector<int>>& ret_embeddings, bool cand_set_provided) {
    ret_embeddings.clear();

    if (cand_set_provided == false) {
        Filter();
    }

    if (cand_.empty()) {
        return;
    }

    GetOrder();

    for (int i = 0; i < q_->getVerticesCount(); ++i) {
        std::vector<int> bn;
        for (int j = 0; j < i; ++j) {
            if (q_->checkEdgeExistence(order_[i], order_[j])) {
                bn.push_back(j);
            }
        }
        back_nbr_poss_.push_back(bn);
    }

    state_cnt_ = 0;
    std::vector<int> embedding;
    vis_.resize(g_->getVerticesCount(), false);
    embedding.reserve(q_->getVerticesCount());
    for (auto u : cand_[order_[0]]) {
        vis_[u] = true;
        embedding.push_back(u);
        Dfs(1, embedding, ret_embeddings);
        embedding.pop_back();
        vis_[u] = false;
        if (ret_embeddings.size() >= embedding_limit_) break;
        if (state_cnt_ >= state_limit_) break;
    }

    for (auto& emb : ret_embeddings) {
        std::vector<int> new_emb;
        new_emb.resize(q_->getVerticesCount());
        for (int i = 0; i < q_->getVerticesCount(); ++i) {
            new_emb[order_[i]] = emb[i];
        }
        emb = new_emb;
    }

    std::cout << "state_cnt_: " << state_cnt_ << std::endl;
}