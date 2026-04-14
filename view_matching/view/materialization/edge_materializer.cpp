#include "edge_materializer.h"

#include "MyUtils.h"
#include "global_vars.h"
#include "multilayer_trie.h"
#include "pretty_print.h"

// #define CHECK_EDGE

void EdgeMaterializer::GroupEmbeddings(unsigned su, unsigned sv, const std::vector<int>& kp, std::map<std::vector<int>, unsigned>& k2g, std::vector<std::vector<int>>& g2k,
                                       SuperDataEdge::Index& index) {
    auto& sdn = sdns_[su];
    for (unsigned i = 0; i < sdn.GetEmbeddingNum(); ++i) {
        std::vector<int> key;
        key.reserve(kp.size());
        for (auto kpp : kp) {
            key.push_back(sdn.GetEmbedding(i)[kpp]);
        }
        unsigned group_id = -1;
        if (k2g.count(key) == 0) {
            k2g[key] = g2k.size();
            g2k.push_back(key);
            group_id = g2k.size() - 1;
        } else {
            group_id = k2g[key];
        }
        index.embedding_id2group_id_.push_back(group_id);
    }
    int g_n = g2k.size();
    index.group_id2embedding_ids_.resize(g_n);
    for (unsigned i = 0; i < sdn.GetEmbeddingNum(); ++i) {
        index.group_id2embedding_ids_[index.embedding_id2group_id_[i]].push_back(i);
    }
}

void EdgeMaterializer::GetKeyPosition(unsigned su, unsigned sv, std::vector<int>& kp, std::vector<int>& kv) {
    auto& e = sqg_.adj_mat_[su][sv];
    for (auto u : e.src_set) {
        kp.push_back(GetPos(sqg_.sqns_[su].order_, u));
        kv.push_back(u);
    }
}

void EdgeMaterializer::GetKeyLink(unsigned su, unsigned sv, std::vector<int>& su_kv, std::vector<int>& sv_kv, std::vector<std::vector<int>>& klk) {
    auto& e = sqg_.adj_mat_[su][sv];
    klk.resize(su_kv.size());
    for (auto [u, v] : e.src_dst_pairs) {
        auto u_pos = GetPos(su_kv, (int)u);
        auto v_pos = GetPos(sv_kv, (int)v);
        assert(u_pos != -1);
        assert(v_pos != -1);
        klk[u_pos].push_back(v_pos);
    }
}

void DfsOnTrie(MultiLayerTrie* mlt, unsigned cur_layer, unsigned idx, std::vector<std::vector<unsigned>>& candidate_sets, std::vector<int>& res) {
    if (cur_layer >= mlt->layer_num_) {
        res.push_back(mlt->row_ids_[idx]);
        return;
    }
    std::vector<int> next_idxs;
    auto start = mlt->offset_[cur_layer - 1][idx];
    auto col_ptr = mlt->columns_[cur_layer].data() + start;
    auto len = mlt->offset_[cur_layer - 1][idx + 1] - mlt->offset_[cur_layer - 1][idx];

    std::unique_ptr<int[]> next_idxs_ptr(new int[len]);
    int next_idxs_n;
    Intersection(col_ptr, len, candidate_sets[cur_layer].data(), candidate_sets[cur_layer].size(), next_idxs_ptr.get(), next_idxs_n);
    for (int i = 0; i < next_idxs_n; ++i) {
        DfsOnTrie(mlt, cur_layer + 1, next_idxs_ptr[i] + start, candidate_sets, res);
    }
}

void EdgeMaterializer::BuildTrie(std::map<std::vector<int>, unsigned>& k2g, std::vector<int>& kp, MultiLayerTrie*& mlt) {
    // build trie on keys of super node sv
    int key_num = k2g.size();
    int key_len = kp.size();
    int layer_num = key_len;
    std::vector<unsigned*> rows;
    std::vector<unsigned> row_ids;
    // unsigned* pool = new unsigned[key_num * key_len];
    std::unique_ptr<unsigned[]> pool(new unsigned[key_num * key_len]);
    unsigned* pool_ptr = pool.get();
    for (auto item : k2g) {
        auto key = item.first;
        memcpy(pool_ptr, key.data(), sizeof(unsigned) * key_len);
        rows.push_back(pool_ptr);
        row_ids.push_back(item.second);
        pool_ptr += key_len;
    }
    mlt = new MultiLayerTrie(layer_num, rows, row_ids);
}

auto EdgeMaterializer::GetCandidateSets(int sv_key_len, std::vector<std::vector<int>>& sv_klk, std::vector<int>& su_kv, std::vector<int>& sv_kv, std::vector<int>& key) {
    std::vector<std::vector<unsigned>> candidate_sets;
    for (int j = 0; j < sv_key_len; ++j) {
        auto sv_pos = j;
        candidate_sets.emplace_back();
        auto& candidate_set = candidate_sets.back();
        for (int k = 0; k < sv_klk[j].size(); ++k) {
            auto su_pos = sv_klk[j][k];
            auto query_u = su_kv[su_pos];
            auto query_v = sv_kv[sv_pos];
            auto match_u = key[su_pos];
            auto match_u_pos = std::lower_bound(cs_.vcand_[query_u].begin(), cs_.vcand_[query_u].end(), match_u) - cs_.vcand_[query_u].begin();
            assert(match_u_pos < cs_.vcand_[query_u].size() && cs_.vcand_[query_u][match_u_pos] == match_u);
            if (candidate_set.empty()) {
                candidate_set = cs_.ecand_[query_u][query_v][match_u_pos];
            } else {
                IntersectSortedLists(candidate_set, cs_.ecand_[query_u][query_v][match_u_pos]);
            }
            if (candidate_set.empty()) {
                candidate_sets.clear();
                return candidate_sets;
            }
        }
    }
#ifndef HOMOMORPHIC
    // eliminate the candidate appeared in the key
    for (auto& candidate_set : candidate_sets) {
        std::vector<unsigned> new_candidate_set;
        for (auto candidate : candidate_set) {
            if (std::find(key.begin(), key.end(), candidate) == key.end()) {
                new_candidate_set.push_back(candidate);
            }
        }
        candidate_set = std::move(new_candidate_set);
    }
#endif
    return candidate_sets;
}

std::vector<int> EdgeMaterializer::LinkOneGroup2NGroups(std::vector<int>& key, int sv_key_len, std::vector<std::vector<int>>& sv_klk, std::vector<int>& su_kv,
                                                        std::vector<int>& sv_kv, MultiLayerTrie* mlt) {
    // GetCandidateSets
    std::vector<std::vector<unsigned>> candidate_sets;
    candidate_sets = GetCandidateSets(sv_key_len, sv_klk, su_kv, sv_kv, key);
    if (candidate_sets.empty()) return {};
    // DfsOnTrie
    std::vector<int> link;
    int idxs_n;
    idxs_.resize(std::min(mlt->columns_[0].size(), candidate_sets[0].size()));
    Intersection(mlt->columns_[0].data(), mlt->columns_[0].size(),
                 candidate_sets[0].data(), candidate_sets[0].size(), idxs_.data(), idxs_n);
    for (int i = 0; i < idxs_n; ++i) {
        DfsOnTrie(mlt, 1, idxs_[i], candidate_sets, link);
    }
    return link;
}

void EdgeMaterializer::LinkG2GEdge(unsigned su, unsigned sv, std::vector<int>& su_kp, std::vector<int>& sv_kp, std::vector<int>& su_kv, std::vector<int>& sv_kv,
                                   std::vector<std::vector<int>>& sv_klk, std::vector<std::vector<unsigned>>& su_g2e,
                                   std::map<std::vector<int>, unsigned>& sv_k2g, std::vector<std::vector<int>>& link,
                                   std::vector<bool>& materialized) {
    // build trie
    MultiLayerTrie* mlt1 = nullptr;
    BuildTrie(sv_k2g, sv_kp, mlt1);
    std::unique_ptr<MultiLayerTrie> mlt(mlt1);

    // link g2g edge
    int sv_key_len = sv_kp.size();
    int su_key_num = su_g2e.size();
    int su_key_len = su_kp.size();
    link.resize(su_key_num);
    materialized.resize(su_key_num, false);
    materialized.assign(su_key_num, false);
    int edge_num = 0;
    std::vector<int> key;
    key.reserve(su_key_len);
    bool& all_materialized = su < sv ? stats_[su][sv].src_all_materialized : stats_[sv][su].dst_all_materialized;
    all_materialized = true;

    for (int i = 0; i < su_key_num; ++i) {
        bool no_complete_embedding = true;
        for (auto embedding_in_group : su_g2e[i]) {
            if (sdns_[su].complete_[embedding_in_group]) {
                no_complete_embedding = false;
                break;
            }
        }
        if (no_complete_embedding) continue;

        GetKeyByPos(sdns_[su].GetEmbedding(su_g2e[i][0]), su_kp, key);
        link[i] = LinkOneGroup2NGroups(key, sv_key_len, sv_klk, su_kv, sv_kv, mlt.get());
        edge_num += link[i].size();

        if (link[i].size() == 0) {
            for (auto embedding_in_group : su_g2e[i]) {
                sdns_[su].complete_[embedding_in_group] = false;
            }
        }

#ifndef CHECK_EDGE
        if (edge_num > MAX_EDGE_NUM) {
            std::cout << "edge" << su << ", " << sv << " edge num exceed limit\n";
            all_materialized = false;
            break;
        }
#endif
        materialized[i] = true;
    }
    // std::cout << "edge num: " << edge_num << std::endl;
}

void EdgeMaterializer::LinkSuperEdge(unsigned su, unsigned sv, bool is_td) {
    std::vector<std::vector<int>> su_g2k, sv_g2k;  // group id to key
    std::vector<int> su_kp, sv_kp,                 // key position
        su_kn, sv_kn;                              // corresponding node id of key
    std::vector<std::vector<int>> su_klk, sv_klk;  // key link, klk[i] = j means the i-th node in su is linked to
                                                   // the j-th node in sv
    std::map<std::vector<int>, unsigned> su_k2g, sv_k2g;
    auto& su_g2e = ses_[su][sv].su_index_.group_id2embedding_ids_;
    auto& sv_g2e = ses_[su][sv].sv_index_.group_id2embedding_ids_;

    auto& se = ses_[su][sv];
    se.su_ = su;
    se.sv_ = sv;

    GetKeyPosition(su, sv, su_kp, su_kn);
    GetKeyPosition(sv, su, sv_kp, sv_kn);

    if (!is_td) {
        GetKeyLink(su, sv, su_kn, sv_kn, su_klk);
        GetKeyLink(sv, su, sv_kn, su_kn, sv_klk);
    }

    GroupEmbeddings(su, sv, su_kp, su_k2g, su_g2k, se.su_index_);
    GroupEmbeddings(sv, su, sv_kp, sv_k2g, sv_g2k, se.sv_index_);

    if (!is_td) {
        LinkG2GEdge(su, sv, su_kp, sv_kp, su_kn, sv_kn, sv_klk, su_g2e, sv_k2g, se.linked_groups_[0], se.materialized_[0]);
        LinkG2GEdge(sv, su, sv_kp, su_kp, sv_kn, su_kn, su_klk, sv_g2e, su_k2g, se.linked_groups_[1], se.materialized_[1]);
    } else {
        se.linked_groups_[0].resize(su_g2k.size());
        se.linked_groups_[1].resize(sv_g2k.size());
        for (auto [key, group_id] : su_k2g) {
            if (sv_k2g.count(key)) se.linked_groups_[0][group_id] = std::vector<int>(1, sv_k2g[key]);
        }
        for (auto [key, group_id] : sv_k2g) {
            if (su_k2g.count(key)) se.linked_groups_[1][group_id] = std::vector<int>(1, su_k2g[key]);
        }
    }

    stats_[su][sv].src_embedding_num = sdns_[su].GetEmbeddingNum();
    stats_[su][sv].src_group_num = se.su_index_.group_id2embedding_ids_.size();
    stats_[su][sv].dst_embedding_num = sdns_[sv].GetEmbeddingNum();
    stats_[su][sv].dst_group_num = se.sv_index_.group_id2embedding_ids_.size();
    stats_[su][sv].src_linked_group_num = 0;
    for (auto lg : se.linked_groups_[0]) {
        stats_[su][sv].src_linked_group_num += lg.size();
    }
    stats_[su][sv].dst_linked_group_num = 0;
    for (auto lg : se.linked_groups_[1]) {
        stats_[su][sv].dst_linked_group_num += lg.size();
    }
}

bool EdgeMaterializer::CheckEdgeBF(unsigned su, unsigned sv, int u_e, int v_e) {
    auto u_embedding = sdns_[su].GetEmbedding(u_e);
    auto v_embedding = sdns_[sv].GetEmbedding(v_e);
    auto se = sqg_.adj_mat_[su][sv];
    for (auto [u, v] : se.src_dst_pairs) {
        auto u_pos = sqg_.sqns_[su].pos_[u];
        auto v_pos = sqg_.sqns_[sv].pos_[v];
        auto u_val = u_embedding[u_pos];
        auto v_val = v_embedding[v_pos];
        auto u_idx = std::lower_bound(cs_.vcand_[u].begin(), cs_.vcand_[u].end(), u_val) - cs_.vcand_[u].begin();
        assert(u_idx < cs_.vcand_[u].size());
        auto tmp = std::lower_bound(cs_.ecand_[u][v][u_idx].begin(), cs_.ecand_[u][v][u_idx].end(), v_val);
        if (tmp == cs_.ecand_[u][v][u_idx].end() || (*tmp) != v_val) return false;
    }
    return true;
}

bool EdgeMaterializer::CheckEdge1(unsigned su, unsigned sv, int u_e, int v_e) {
    bool d = su > sv;  // 0: su --> sv, 1: sv --> su
    auto& se = d ? ses_[sv][su] : ses_[su][sv];
    auto& src_index = d ? se.sv_index_ : se.su_index_;
    auto& dst_index = d ? se.su_index_ : se.sv_index_;
    int group_id = src_index.embedding_id2group_id_[u_e];
    auto& link = se.linked_groups_[d];
    for (auto v_g : link[group_id]) {
        for (auto v_e1 : dst_index.group_id2embedding_ids_[v_g]) {
            if (v_e1 == v_e) return true;
        }
    }
    return false;
}

bool EdgeMaterializer::NoRepeat(unsigned su, unsigned sv, int u_eid, int v_eid) {
    auto u_e = sdns_[su].GetEmbedding(u_eid);
    auto v_e = sdns_[sv].GetEmbedding(v_eid);
    auto u_e_len = sqg_.sqns_[su].GetVarNum();
    auto v_e_len = sqg_.sqns_[sv].GetVarNum();
    for (int i = 0; i < u_e_len; ++i) {
        for (int j = 0; j < v_e_len; ++j) {
            if (u_e[i] == v_e[j]) return false;
        }
    }
    return true;
}

bool EdgeMaterializer::CheckNewSuperEdge(unsigned su, unsigned sv) {
    for (int i = 0; i < sdns_[su].GetEmbeddingNum(); ++i) {
        auto u_embedding_len = sqg_.sqns_[su].GetVarNum();
        for (int j = 0; j < sdns_[sv].GetEmbeddingNum(); ++j) {
            auto v_embedding_len = sqg_.sqns_[sv].GetVarNum();
            bool res0 = CheckEdgeBF(su, sv, i, j);
            bool res1 = CheckEdge1(su, sv, i, j);
            if (NoRepeat(su, sv, i, j) && res0 != res1) {
                std::cout << "CheckNewSuperEdge failed\n";
                std::cout << "su: " << su << " sv: " << sv << "\n";
                std::cout << "BF: " << res0 << " res1: " << res1 << std::endl;
                std::cout << "u" << i << ": " << std::vector<unsigned>(sdns_[su].GetEmbedding(i), sdns_[su].GetEmbedding(i) + u_embedding_len) << std::endl;
                std::cout << "v" << j << ": " << std::vector<unsigned>(sdns_[sv].GetEmbedding(j), sdns_[sv].GetEmbedding(j) + v_embedding_len) << std::endl;
                exit(1);
            }
        }
    }
    return true;
}

void EdgeMaterializer::LinkSuperEdge(bool is_td) {
    stats_.resize(sqg_.sqns_.size());
    stats_.assign(sqg_.sqns_.size(), std::vector<Stats>(sqg_.sqns_.size()));
    for (unsigned i = 0; i < sqg_.sqns_.size(); ++i) {
        auto u = i;
        for (unsigned j = 0; j < sqg_.adj_[u].size(); ++j) {
            auto v = sqg_.adj_[u][j].dst;
            if (u > v) continue;
            // std::cout << "linking super edge " << u << " " << v << "\n";
            LinkSuperEdge(u, v, is_td);
        }
    }

    for (unsigned i = 0; i < sqg_.sqns_.size(); ++i) {
        auto u = i;
        for (unsigned j = 0; j < sqg_.adj_[u].size(); ++j) {
            auto v = sqg_.adj_[u][j].dst;
            if (u > v) continue;
            if (log_str != nullptr) {
                std::stringstream ss("");
                ss << "stats of super edge " << u << " " << v << ":\n";
                ss << "  src_embedding_num: " << stats_[u][v].src_embedding_num << "\n";
                ss << "  src_group_num: " << stats_[u][v].src_group_num << "\n";
                ss << "  dst_embedding_num: " << stats_[u][v].dst_embedding_num << "\n";
                ss << "  dst_group_num: " << stats_[u][v].dst_group_num << "\n";
                ss << "  src_all_materialized: " << stats_[u][v].src_all_materialized << "\n";
                ss << "  dst_all_materialized: " << stats_[u][v].dst_all_materialized << "\n";
                ss << "  src_linked_group_num: " << stats_[u][v].src_linked_group_num << "\n";
                ss << "  dst_linked_group_num: " << stats_[u][v].dst_linked_group_num << "\n";
                *log_str += ss.str();
            }
        }
    }

#ifdef CHECK_EDGE
    std::cout << "checking edge...\n";
    for (unsigned i = 0; i < sqg_.sqns_.size(); ++i) {
        auto u = i;
        for (unsigned j = 0; j < sqg_.adj_[u].size(); ++j) {
            auto v = sqg_.adj_[u][j].dst;
            // std::cout << "check edge u: " << u << " v: " << v << "\n";
            assert(CheckNewSuperEdge(u, v));
        }
    }
#endif
}
