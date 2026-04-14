#include "enumerate.h"

#include <bitset>
#include <cmath>
#include <future>

#include "MyUtils.h"
#include "encoder.h"
#include "enumerate.h"
#include "global_vars.h"
#include "preprocessor.h"
#include "primitive/projection.h"
#include "query_plan_generator.h"
#include "query_plan_generator1.h"
#include "relation/catalog.h"
#include "rm_filter.h"
#include "utility/computesetintersection.h"
#include "view/edge_iterator.h"

#define FAILING_SET
#define USE_MAT_EDGE

bool Enumerator::CheckEmbedding(ui* embedding, ui len) {
    auto query_graph = aux_.query_graph_;
    auto data_graph = aux_.data_graph_;
    for (int i = 0; i < len; ++i) {
        auto qu = aux_.query_node_order_[i];
        for (int j = i + 1; j < len; ++j) {
            auto qv = aux_.query_node_order_[j];
            if (query_graph->checkEdgeExistence(qu, qv)) {
                auto u = embedding[i];
                auto v = embedding[j];
                if (u == -1 || v == -1) continue;
                if (data_graph->checkEdgeExistence(u, v) == false) {
                    std::cout << "check embedding failed: " << qu << " - " << qv << " (" << u << ", " << v << ")" << std::endl;
                    return false;
                }
            }
        }
    }
    return true;
}

void Enumerator::GetLocalCandidate(EnumerateState& state, unsigned query_node) {
    bool first = true;
    auto& candidate_buffer = state.candidate_buffers_[query_node];
    auto& si_buffer = state.candidate_temps_[query_node];
    unsigned& candidate_buffer_size = state.candidate_buffer_size_[query_node];
    candidate_buffer_size = 0;
    unsigned si_buffer_size = 0;
    for (auto pa_query_node : aux_.parent_query_nodes_[query_node]) {
        auto pa_query_node_no = aux_.query_node_no_[pa_query_node];
        unsigned len;
        const unsigned* nbrs;
        auto idx_pa_data_node = state.idx_embedding_[pa_query_node_no];
        nbrs = aux_.storage_->get_core_relation_children(pa_query_node, query_node, idx_pa_data_node, len);
        get_local_candidate_count_ += len;
        if (first) {
            memcpy(candidate_buffer.get(), nbrs, len * sizeof(unsigned));
            candidate_buffer_size = len;
            first = false;
        } else {
            ComputeSetIntersection::ComputeCandidates(candidate_buffer.get(), candidate_buffer_size, nbrs, len, si_buffer.get(), si_buffer_size);
            std::swap(candidate_buffer, si_buffer);
            candidate_buffer_size = si_buffer_size;
        }
        if (candidate_buffer_size == 0) break;
    }
}

void Enumerator::UpdateFailingSetByConflictClass(EnumerateState& state, unsigned u, unsigned u1, unsigned& last_fs) {
#ifdef FAILING_SET
    auto cur_fs = state.anc_[u1] | state.anc_[u];
    last_fs |= cur_fs;
    cur_fs = 0;
#endif
    ++conflict_count_;
}

bool Enumerator::CheckPatchEdges(RewriteQuery::Node* mat_node, unsigned dst_embedding_id) {
    auto sdn = mat_node->sdn_;
    auto embedding = sdn->GetEmbedding(dst_embedding_id);
    for (auto patch_edge : mat_node->patch_edges1_) {
        auto data_u = embedding[patch_edge.first];
        auto data_v = embedding[patch_edge.second];
        if (aux_.data_graph_->checkEdgeExistence(data_u, data_v) == false) {
            return false;
        }
    }
    return true;
}

void Enumerator::CalcEncodeEmbedding(RewriteQuery::Node* mat_node, unsigned dst_embedding_id) {
    calc_encode_count_++;
    auto embedding_width = mat_node->GetInnerOrder().size();
    auto sdn = mat_node->sdn_;
    auto embedding = sdn->GetEmbedding(dst_embedding_id);
    for (int j = 0; j < embedding_width; ++j) {
        auto query_node = mat_node->GetInnerOrder()[j];

        if ((aux_.need_idx_ & (1ULL << query_node)) == 0) {
            continue;
        }

        auto data_node = embedding[j];
        unsigned idx = aux_.storage_->get_candidate_index(query_node, data_node);
        if (idx == aux_.storage_->get_num_candidates(query_node)) {
            mat_node->encoded_embeddings_[dst_embedding_id] = nullptr;
            break;
        }
        mat_node->encoded_embeddings_[dst_embedding_id][j] = idx;
    }
    if (mat_node->encoded_embeddings_[dst_embedding_id] == nullptr) return;
    if (!CheckPatchEdges(mat_node, dst_embedding_id)) {
        mat_node->encoded_embeddings_[dst_embedding_id] = nullptr;
    }
}

void Enumerator::EnumerateOnTrie(int cur, int trie_id, int last_not_empty, unsigned& last_fs, ui need_check_vis,
                                 int embedding_width, NewTrie* trie, const std::vector<unsigned>& order, EnumerateState& state, std::vector<ui>& lc) {
    unsigned query_node = order[cur];
    unsigned start = 0, len = 0;

    auto& candidate_size = state.candidate_buffer_size_[query_node];
    auto& candidate_buffer = state.candidate_buffers_[query_node];

    if (candidate_size > 0) {
        for (int i = 0; i < candidate_size; ++i) {
            auto v = candidate_buffer[i];
            // check conflict
            if (need_check_vis & (1 << cur)) {
                int conflict_query_node = state.vis1_->Get(v);
                if (conflict_query_node != -1) {
                    UpdateFailingSetByConflictClass(state, query_node, conflict_query_node, last_fs);
                    continue;
                }
            }
            auto it = trie->pool_[trie_id].children_.find(v);
            if (it == trie->pool_[trie_id].children_.end()) {
                continue;
            }
            auto child_idx = it->second;
            if (cur == embedding_width - 1) {
                lc.push_back(trie->pool_[child_idx].row_id_);
            } else {
                state.cur_offset_++;
                EnumerateOnTrie(cur + 1, child_idx, last_not_empty, last_fs, need_check_vis, embedding_width, trie, order, state, lc);
                state.cur_offset_--;
            }
        }
    } else {
        for (auto& [child_value, child_idx] : trie->pool_[trie_id].children_) {
            auto v = child_value;
            // check conflict
            if (need_check_vis & (1 << cur)) {
                int conflict_query_node = state.vis1_->Get(v);
                if (conflict_query_node != -1) {
                    UpdateFailingSetByConflictClass(state, query_node, conflict_query_node, last_fs);
                    continue;
                }
            }
            if (cur == embedding_width - 1) {
                lc.push_back(trie->pool_[child_idx].row_id_);
            } else {
                state.cur_offset_++;
                EnumerateOnTrie(cur + 1, child_idx, last_not_empty, last_fs, need_check_vis, embedding_width, trie, order, state, lc);
                state.cur_offset_--;
            }
        }
    }
}

bool Enumerator::HandleSingleNode(ui rw_node_id, EnumerateState& state, ui& last_fs, ui& cur_fs, ui sum, ui query_node, std::vector<ui>& super_node_candidates) {
    bool have_answer = false;
    auto nec_num = aux_.rq_nec_count_[rw_node_id];
    if (sum < aux_.rq_nec_count_[rw_node_id]) {
        last_fs |= state.anc_[query_node];
        return have_answer;
    }

    // enumerate
    if (aux_.port_vertices_[rw_node_id].size() == 0) {
        long double perm_num = Perm(sum, nec_num);
        state.product_ *= perm_num;
        if (state.cur_depth_ != aux_.rw_order_.size() - nec_num) {
            state.cur_depth_ += nec_num;
            state.cur_offset_ += nec_num;
            have_answer |= EnumerateDfs(state, cur_fs);
            state.cur_depth_ -= nec_num;
            state.cur_offset_ -= nec_num;
            if (state.finished_) return have_answer;
            if (!have_answer) {
                last_fs = cur_fs;  // attention: failure comes from children
            }
        } else {
            have_answer = true;
            state.embedding_cnt_ += state.product_;
            if (aux_.num_limit_ != -1 && state.embedding_cnt_ >= aux_.num_limit_) {
                state.finished_ = true;
                return have_answer;
            }
        }
        state.product_ /= perm_num;
    } else {
        state.product_ *= Perm(nec_num, nec_num);

        auto comb = Combination(sum, nec_num);
        for (comb.Init(); !comb.End();) {
            auto comb_indices = comb.Data();
            auto offset = state.cur_offset_;

            // update state
            if (super_node_candidates.empty()) {
                if (aux_.need_idx_ & (1ULL << query_node)) {
                    for (int i = 0; i < nec_num; ++i) {
                        auto v = state.candidate_buffers_[query_node][comb_indices[i]];
                        auto iv = aux_.storage_->get_candidate_index(query_node, v);
                        state.Update1(iv, v, query_node);
                    }
                } else {
                    for (int i = 0; i < nec_num; ++i) {
                        auto v = state.candidate_buffers_[query_node][comb_indices[i]];
                        state.Update1(-1, v, query_node);
                    }
                }
            } else {
                for (int i = 0; i < nec_num; ++i) {
                    auto super_node_candidate_id = super_node_candidates[comb_indices[i]];
                    auto embedding = rq_.nodes_[rw_node_id]->sdn_->GetEmbedding(super_node_candidate_id);
                    auto rw_node_id1 = aux_.rw_order_[state.cur_depth_];
                    // we need the embedding_id of current super-node, so use UpdateK
                    state.UpdateK(embedding, 1, rw_node_id1, rq_.nodes_[rw_node_id], super_node_candidate_id);
                }
            }

            have_answer |= EnumerateDfs(state, cur_fs);

            // restore state
            if (super_node_candidates.empty()) {
                for (int i = 0; i < nec_num; ++i) state.Restore1();
            } else {
                for (int i = 0; i < nec_num; ++i) state.RestoreK(1);
            }

            if (state.finished_) return have_answer;

            if (!have_answer) {
                // check failing set
                int fail_pos = -1;
                for (int i = nec_num - 1; i >= 0; --i) {
                    if (cur_fs & (1 << aux_.query_node_order_[i + offset])) {
                        fail_pos = i;
                        break;
                    }
                }
                // update failing set
                if (fail_pos == -1) {
                    last_fs = cur_fs;
                    break;  // skip the rest
                } else {
                    last_fs |= cur_fs;
                    cur_fs = 0;
                }
                comb.Next(fail_pos);  // change the value at fail_pos
            } else {
                comb.Next();
            }
        }

        state.product_ /= Perm(nec_num, nec_num);
    }
    return have_answer;
}

bool Enumerator::EnumerateDfs(EnumerateState& state, unsigned& last_fs) {
    // state.Print();
    //  assert(CheckEmbedding(state.embedding_.get(), state.cur_offset_));

    state.state_cnt1_[state.cur_offset_]++;
    state.state_cnt_++;

    last_fs = 0;

    bool have_answer = false;
    auto rw_node_id = aux_.rw_order_[state.cur_depth_];
    // if (rw_node->type_ == RewriteQuery::Type::NORMAL)
    if (aux_.enum_methods_[rw_node_id] == EnumMethod::NormalNode) {
        have_answer = HandleNormalNode(state, last_fs);
    } else {
        have_answer = HandleMatNode(state, last_fs);
    }

    state.fail_state_cnt_ += !have_answer;
    state.fail_state_cnt1_[state.cur_depth_] += !have_answer;

    if (g_exit) {
        state.finished_ = true;
        return true;
    }
    return have_answer;
}

void Enumerator::CheckLC(EnumerateState& state, unsigned& last_fs, std::vector<ui>& lc, RewriteQuery::Node* mat_node, int embedding_width, ui rw_node_id) {
    // auto rw_node_id = aux_.rw_order_[state.cur_depth_];
    // auto rw_node = rq_.nodes_[rw_node_id];
    // auto mat_node = rw_node;
    // auto sqn = mat_node->sqn_;
    // int embedding_width = sqn->GetEmbeddingWidth();
    // std::vector<ui> need_check_vis;
    // for (int j = 0; j < embedding_width; ++j) {
    //     auto query_node = mat_node->GetInnerOrder()[j];
    //     bool need_check = false;
    //     for (int k = 0; k < state.cur_depth_; ++k) {
    //         auto rw_node_id1 = aux_.rw_order_[k];
    //         auto rw_node1 = rq_.nodes_[rw_node_id1];
    //         for (int t = 0; t < rw_node1->GetNodeNum(); ++t) {
    //             auto u = rw_node1->GetInnerOrder()[t];
    //             if (aux_.query_graph_->getVertexLabel(query_node) == aux_.query_graph_->getVertexLabel(u)) {
    //                 need_check = true;
    //                 // check if u and query_node are linked by mat edge and both are in port vertices
    //                 for (auto parent_rev_mat_edge : aux_.parent_rev_edges_[rw_node_id]) {
    //                     if (parent_rev_mat_edge->src_ != rw_node_id1) continue;
    //                     auto parent_mat_edge = (RewriteQuery::MatEdge*)parent_rev_mat_edge;
    //                     if (GetPos(parent_mat_edge->src_port_vertices_, u) != -1 &&
    //                         GetPos(parent_mat_edge->dst_port_vertices_, query_node) != -1) {
    //                         need_check = false;
    //                         break;
    //                     }
    //                 }
    //                 if (need_check) break;
    //             }
    //         }
    //         if (need_check) break;
    //     }
    //     if (need_check) {
    //         need_check_vis.push_back(j);
    //     }
    // }
    int new_lc_size = 0;
    unsigned long long sketch = state.vis1_->sketch_;
    for (int i = 0; i < lc.size(); ++i) {
        auto dst_embedding_id = lc[i];
        if (mat_node->patch_edges1_.size() > 0 && aux_.port_vertices_[rw_node_id].size() == 0) {
            auto& pass = mat_node->patch_edge_passed_[dst_embedding_id];
            if (pass == 0) {
                pass = CheckPatchEdges(mat_node, dst_embedding_id) ? 1 : -1;
            }
            if (pass == -1) continue;
        }
        // check vis
        bool flag = true;
        auto sdn = mat_node->sdn_;
        auto embedding = sdn->GetEmbedding(dst_embedding_id);
        int v, u = -1, u1 = -1;
        // for (auto j : need_check_vis) {
        for (int j = 0; j < embedding_width; ++j) {
            v = embedding[j];
            int block = v >> state.vis1_->width_;
            if (sketch & (1ULL << block)) {
                check_vis_count_++;
                int vis = state.vis1_->Get(v);
                if (vis != -1) {
                    u = mat_node->GetInnerOrder()[j];
                    u1 = vis;
                    break;
                }
            }
        }
        if (u != -1) {
            UpdateFailingSetByConflictClass(state, u, u1, last_fs);
            flag = false;
        }
        if (flag) {
            lc[new_lc_size++] = dst_embedding_id;
        }
    }
    lc.resize(new_lc_size);
}

bool Enumerator::HandleMatNode(EnumerateState& state, unsigned int& last_fs) {
    auto rw_node_id = aux_.rw_order_[state.cur_depth_];
    auto rw_node = rq_.nodes_[rw_node_id];
    bool have_answer = false;
    auto mat_node = rw_node;
    auto sdn = mat_node->sdn_;
    auto sqn = mat_node->sqn_;
    int embedding_width = sqn->GetEmbeddingWidth();
    auto& cur_fs = state.fs_[state.cur_offset_];

    // check if we can enumerate by mat edge
    bool enumerate_by_mat_edge = false;
#ifdef USE_MAT_EDGE
    if (aux_.enum_methods_[rw_node_id] == EnumMethod::MatEdge) {
        enumerate_by_mat_edge = true;
    } else if (aux_.enum_methods_[rw_node_id] == EnumMethod::Mix) {
        enumerate_by_mat_edge = true;
        for (auto pa_rev_edge : aux_.parent_rev_edges_[rw_node_id]) {
            auto pa_rev_mat_edge = (RewriteQuery::MatEdge*)pa_rev_edge;
            auto& pa_rw_node_id = pa_rev_mat_edge->src_;
            auto& pa_sdn_ebd_id = state.sdn_embedding_ids_[pa_rw_node_id];
            auto& se = pa_rev_mat_edge->super_edge_cand_;
            if (!se->IsMaterialized(pa_rev_mat_edge->d_, pa_sdn_ebd_id)) {
                enumerate_by_mat_edge = false;
                break;
            }
        }
    }
#endif

    // get local candidates
    std::vector<ui>& lc = lcs_[rw_node_id];
    lc.clear();
    if (enumerate_by_mat_edge) {
        for (auto parent_rev_mat_edge : aux_.parent_rev_edges_[rw_node_id]) {
            auto parent_mat_edge = (RewriteQuery::MatEdge*)parent_rev_mat_edge;
            auto src_embedding_id = state.sdn_embedding_ids_[parent_mat_edge->src_];

            std::vector<ui>& tmp_lc = tmp_lcs_[rw_node_id];
            tmp_lc.clear();

            auto se = SuperDataEdgeView(*parent_mat_edge->super_edge_cand_, parent_mat_edge->d_);
            auto gid = se.su_index_.embedding_id2group_id_[src_embedding_id];
            for (auto dst_gid : se.linked_groups_[gid]) {
                tmp_lc.insert(tmp_lc.end(),
                              se.sv_index_.group_id2embedding_ids_[dst_gid].begin(),
                              se.sv_index_.group_id2embedding_ids_[dst_gid].end());
            }

            if (lc.empty()) {
                std::swap(lc, tmp_lc);
            } else {
                if (lc.size() > tmp_lc.size()) std::swap(lc, tmp_lc);
                for (auto v : lc) intersection_bs_->Set(v);
                int new_tmp_lc_size = 0;
                for (int i = 0; i < tmp_lc.size(); ++i) {
                    if (intersection_bs_->Test(tmp_lc[i])) {
                        tmp_lc[new_tmp_lc_size++] = tmp_lc[i];
                    }
                }
                tmp_lc.resize(new_tmp_lc_size);
                for (auto v : lc) intersection_bs_->Reset(v);
                std::swap(lc, tmp_lc);
            }
            if (lc.empty()) break;

            if (parent_mat_edge->patch_edges_.empty() == false) {
                auto src_embedding = rq_.nodes_[parent_mat_edge->src_]->sdn_->GetEmbedding(src_embedding_id);
                for (auto [su_pos, sv_pos] : parent_mat_edge->patch_edges_) {
                    int new_lc_size = 0;
                    auto data_u = src_embedding[su_pos];
                    auto& bs = check_edge_bs_;
                    ui len;
                    const ui* nbrs = aux_.data_graph_->getVertexNeighbors(data_u, len);
                    for (int i = 0; i < len; ++i) {
                        bs->Set(nbrs[i]);
                    }
                    for (int i = 0; i < lc.size(); ++i) {
                        auto dst_embedding_id = lc[i];
                        auto data_v = sdn->GetEmbedding(dst_embedding_id)[sv_pos];
                        if (bs->Test(data_v)) {
                            lc[new_lc_size++] = dst_embedding_id;
                        }
                    }
                    for (int i = 0; i < len; ++i) {
                        bs->Reset(nbrs[i]);
                    }
                    lc.resize(new_lc_size);
                }
            }
            if (lc.empty()) break;
        }

        CheckLC(state, last_fs, lc, mat_node, embedding_width, rw_node_id);
    } else {
        // get candidate by back neighbors
        for (int i = 0; i < embedding_width; ++i) {
            auto query_node = mat_node->GetInnerOrder()[i];
            if (aux_.parent_query_nodes_[query_node].empty() == false) {
                GetLocalCandidate(state, query_node);
                if (state.candidate_buffer_size_[query_node] == 0) {
#ifdef FAILING_SET
                    last_fs = state.anc_[query_node];
#endif
                    return have_answer;
                }
            } else {
                state.candidate_buffer_size_[query_node] = 0;
            }
        }

        int last_not_empty = embedding_width - 1;
        for (; last_not_empty >= 0; --last_not_empty) {
            auto query_node = mat_node->GetInnerOrder()[last_not_empty];
            if (state.candidate_buffer_size_[query_node] > 0) break;
        }
        // std::cout << "last_not_empty: " << last_not_empty << std::endl;

        ui need_check_vis = 0;
        for (int j = 0; j < embedding_width; ++j) {
            for (int k = 0; k < state.cur_depth_; ++k) {
                auto rw_node_id1 = aux_.rw_order_[k];
                auto rw_node1 = rq_.nodes_[rw_node_id1];
                for (int t = 0; t < rw_node1->GetNodeNum(); ++t) {
                    auto u = rw_node1->GetInnerOrder()[t];
                    auto query_node = mat_node->GetInnerOrder()[j];
                    if (aux_.query_graph_->getVertexLabel(query_node) == aux_.query_graph_->getVertexLabel(u)) {
                        need_check_vis |= (1 << j);
                        break;
                    }
                }
            }
        }

        EnumerateOnTrie(0, 0, last_not_empty, last_fs, need_check_vis, embedding_width, aux_.tries1_[rw_node_id], mat_node->GetInnerOrder(), state, lc);

        if (mat_node->patch_edges1_.size() > 0) {
            int new_lc_size = 0;
            for (int i = 0; i < lc.size(); ++i) {
                auto dst_embedding_id = lc[i];
                if (aux_.port_vertices_[rw_node_id].size() == 0) {
                    auto& pass = mat_node->patch_edge_passed_[dst_embedding_id];
                    if (pass == 0) {
                        pass = CheckPatchEdges(mat_node, dst_embedding_id) ? 1 : -1;
                    }
                    if (pass == -1) continue;
                }
                lc[new_lc_size++] = dst_embedding_id;
            }
            lc.resize(new_lc_size);
        }
    }
    return EnumerateMatNode(state, last_fs);
}

bool Enumerator::EnumerateMatNode(EnumerateState& state, unsigned& last_fs) {
    auto rw_node_id = aux_.rw_order_[state.cur_depth_];
    auto rw_node = rq_.nodes_[rw_node_id];
    bool have_answer = false;
    auto mat_node = rw_node;
    auto sdn = mat_node->sdn_;
    auto sqn = mat_node->sqn_;
    int embedding_width = sqn->GetEmbeddingWidth();
    auto& cur_fs = state.fs_[state.cur_offset_];
    bool empty = true;

    std::vector<ui>& lc = lcs_[rw_node_id];

    // enumerate
    if (rw_node->GetNodeNum() != 1) {
        if (aux_.port_vertices_[rw_node_id].size() == 0) {
            int sum = lc.size();
            empty = (sum == 0);
            if (sum) {
                state.product_ *= sum;
                if (state.cur_depth_ == aux_.rw_order_.size() - 1) {
                    state.embedding_cnt_ += state.product_;
                    have_answer = true;
                    if (aux_.num_limit_ != -1 && state.embedding_cnt_ >= aux_.num_limit_) {
                        state.finished_ = true;
                        return have_answer;
                    }
                } else {
                    state.cur_depth_++;
                    state.cur_offset_ += embedding_width;
                    have_answer |= EnumerateDfs(state, cur_fs);
                    state.cur_offset_ -= embedding_width;
                    state.cur_depth_--;
                    if (state.finished_) return have_answer;
#ifdef FAILING_SET
                    if (!have_answer) {
                        last_fs = cur_fs;
                    }
#endif
                }
                state.product_ /= sum;
            }
        } else {
            if (aux_.port_vertices_[rw_node_id].size() == mat_node->GetNodeNum()) {
                for (int i = 0; i < lc.size(); ++i) {
                    auto dst_embedding_id = lc[i];

                    auto& encode_embedding = mat_node->encoded_embeddings_[dst_embedding_id];
                    if (encode_embedding == nullptr)
                        continue;
                    else if (encode_embedding[0] == -1) {
                        CalcEncodeEmbedding(mat_node, dst_embedding_id);
                        if (encode_embedding == nullptr) continue;
                    }

                    empty = false;

                    auto embedding = sdn->GetEmbedding(dst_embedding_id);

                    state.UpdateK(embedding, embedding_width, rw_node_id, mat_node, dst_embedding_id);

                    have_answer |= EnumerateDfs(state, cur_fs);

                    state.RestoreK(embedding_width);

                    if (state.finished_) return have_answer;

#ifdef FAILING_SET
                    if (!have_answer) {
                        unsigned flag = 0;
                        for (int i = 0; i < embedding_width; ++i) {
                            auto u = mat_node->GetInnerOrder()[i];
                            if (BIT_TEST(cur_fs, u)) {
                                flag |= (1 << i);
                            }
                        }
                        if (!flag) {
                            last_fs = cur_fs;
                            cur_fs = 0;
                            break;
                        } else {
                            last_fs |= cur_fs;
                            cur_fs = 0;
                            for (; i + 1 < lc.size(); ++i) {
                                auto embedding1 = sdn->GetEmbedding(lc[i + 1]);
                                auto embedding = sdn->GetEmbedding(lc[i]);
                                bool same = true;
                                for (int j = 0; j < embedding_width; ++j) {
                                    if (BIT_TEST(flag, j) && embedding[j] != embedding1[j]) {
                                        same = false;
                                        break;
                                    }
                                }
                                if (!same) break;
                            }
                        }
                    }
#endif
                }
            } else {
                // group candidate by port vertices
                std::map<std::vector<unsigned>, std::pair<ui, ui>> port_group;  // port_vertices -> <start_idx, count>
                std::vector<unsigned> port_vertices_key;
                port_vertices_key.reserve(aux_.port_vertices_[rw_node_id].size());
                for (int i = 0; i < lc.size(); ++i) {
                    auto dst_embedding_id = lc[i];
                    auto& encode_embedding = mat_node->encoded_embeddings_[dst_embedding_id];

                    if (encode_embedding == nullptr)
                        continue;
                    else if (encode_embedding[0] == -1) {
                        CalcEncodeEmbedding(mat_node, dst_embedding_id);
                        if (encode_embedding == nullptr) continue;
                    }

                    auto embedding = sdn->GetEmbedding(dst_embedding_id);
                    port_vertices_key.clear();
                    for (auto pos : aux_.port_vertices_[rw_node_id]) {
                        port_vertices_key.push_back(embedding[pos]);
                    }

                    auto iter = port_group.find(port_vertices_key);
                    if (iter == port_group.end()) {
                        port_group.insert({port_vertices_key, {dst_embedding_id, 1}});
                    } else {
                        iter->second.second++;
                    }
                }

                empty = (port_group.size() == 0);

                // std::cout << "rw_node_id: " << rw_node_id << " port group: " << port_group << std::endl;

                // enumerate for each group
                for (auto it = port_group.begin(); it != port_group.end(); ++it) {
                    ui dst_embedding_id = it->second.first;
                    ui count = it->second.second;

                    auto embedding = sdn->GetEmbedding(dst_embedding_id);
                    state.product_ *= count;
                    state.UpdateK(embedding, embedding_width, rw_node_id, mat_node, dst_embedding_id);

                    have_answer |= EnumerateDfs(state, cur_fs);

                    state.RestoreK(embedding_width);
                    state.product_ /= count;

                    if (state.finished_) return have_answer;

#ifdef FAILING_SET
                    if (!have_answer) {
                        unsigned flag = 0;
                        for (int i = 0; i < embedding_width; ++i) {
                            auto u = mat_node->GetInnerOrder()[i];
                            if (BIT_TEST(cur_fs, u)) {
                                flag |= (1 << i);
                            }
                        }
                        if (!flag) {
                            last_fs = cur_fs;
                            cur_fs = 0;
                            break;
                        } else {
                            last_fs |= cur_fs;
                            cur_fs = 0;

                            auto next_it = it;
                            next_it++;
                            for (; next_it != port_group.end(); it = next_it, next_it++) {
                                auto embedding1 = sdn->GetEmbedding(next_it->second.first);
                                auto embedding = sdn->GetEmbedding(it->second.first);
                                bool same = true;
                                for (int j = 0; j < embedding_width; ++j) {
                                    if (BIT_TEST(flag, j) && embedding[j] != embedding1[j]) {
                                        same = false;
                                        break;
                                    }
                                }
                                if (!same) break;
                            }
                        }
                    }
#endif
                }
            }
        }
    } else {
        // get encoded embedding and check vis
        if (aux_.port_vertices_[rw_node_id].size() > 0) {
            int new_lc_size = 0;
            for (auto dst_embedding_id : lc) {
                auto& encode_embedding = mat_node->encoded_embeddings_[dst_embedding_id];
                if (encode_embedding == nullptr)
                    continue;
                else if (encode_embedding[0] == -1) {
                    CalcEncodeEmbedding(mat_node, dst_embedding_id);
                    if (encode_embedding == nullptr) continue;
                }

                empty = false;
                lc[new_lc_size++] = dst_embedding_id;
            }
            lc.resize(new_lc_size);
        } else {
            empty = (lc.size() == 0);
        }

        if (lc.size()) {
            have_answer = HandleSingleNode(rw_node_id, state, last_fs, cur_fs, lc.size(), rw_node->GetInnerOrder()[0], lc);
        }
    }

    // check empty class
    if (empty) {
        empty_count_++;
#ifdef FAILING_SET
        for (int i = 0; i < embedding_width; ++i) {
            auto u = mat_node->GetInnerOrder()[i];
            last_fs |= state.anc_[u];
        }
#endif
        return false;
    }
    return have_answer;
}

bool Enumerator::HandleNormalNode(EnumerateState& state, unsigned int& last_fs) {
    auto rw_node_id = aux_.rw_order_[state.cur_depth_];
    auto rw_node = rq_.nodes_[rw_node_id];
    auto query_node = rw_node->GetSingleNode();
    unsigned& cur_fs = state.fs_[state.cur_offset_];
    bool have_answer = false;

    // get local candidates
    GetLocalCandidate(state, query_node);

    if (state.candidate_buffer_size_[query_node] == 0) {
        empty_count_++;
#ifdef FAILING_SET
        last_fs = state.anc_[query_node];
#endif
        return false;
    }

    long long sum = 0;
    for (int i = 0; i < state.candidate_buffer_size_[query_node]; ++i) {
        auto candidate = state.candidate_buffers_[query_node][i];
        int conflict_query_node = state.vis1_->Get(candidate);
        if (conflict_query_node != -1 && !have_answer) {
            UpdateFailingSetByConflictClass(state, query_node, conflict_query_node, last_fs);
            continue;
        }
        if (aux_.port_vertices_[rw_node_id].size() == 0)
            sum++;
        else {
            // state.candidate_buffers_[query_node][sum++] = idx_candidate;
            state.candidate_buffers_[query_node][sum++] = candidate;
        }
    }
    state.candidate_buffer_size_[query_node] = sum;

    std::vector<ui> _;
    have_answer = HandleSingleNode(rw_node_id, state, last_fs, cur_fs, sum, query_node, _);
    return have_answer;
}

long double Enumerator::EnumerateFromRewriteQuery(Graph* query_graph, Graph* data_graph, long long num_limit, unsigned* embeddings) {
    // CalcAuxilaryInfo(query_graph, data_graph);
    Preprocessor preprocessor(rq_, aux_, query_graph, data_graph);
    preprocessor.Exec();
    aux_.enable_output = false;
    std::cout << "finished preprocess" << std::endl;

    EnumerateState state;

    Initialize(state);
    aux_.num_limit_ = num_limit;

    auto start = std::chrono::high_resolution_clock::now();
    auto rw_node_id = aux_.rw_order_[0];
    auto rw_node = rq_.nodes_[rw_node_id];
    if (rw_node->type_ == RewriteQuery::Type::NORMAL) {
        auto query_node = rw_node->node_id_;
        auto candidate_cnt = aux_.storage_->get_num_candidates(query_node);
        state.candidate_buffer_size_[query_node] = candidate_cnt;
        if (aux_.port_vertices_[rw_node_id].size() > 0) {
            memcpy(state.candidate_buffers_[query_node].get(), aux_.storage_->get_candidates(query_node), candidate_cnt * sizeof(unsigned));
        }
        std::vector<ui> _;
        ui last_fs;
        ui& cur_fs = state.fs_[0];
        HandleSingleNode(rw_node_id, state, last_fs, cur_fs, candidate_cnt, query_node, _);
    } else {
        auto mat_node = rw_node;
        auto sdn = mat_node->sdn_;
        auto sqn = mat_node->sqn_;
        int embedding_width = sqn->GetEmbeddingWidth();
        auto& cur_fs = state.fs_[0];
        bool have_answer = false;

        auto& lc = lcs_[rw_node_id];
        lc.clear();
        for (int i = 0; i < sdn->GetEmbeddingNum(); ++i) {
            if (sdn->CheckEmbedding(i) == false) continue;
            lc.push_back(i);
        }

        have_answer = EnumerateMatNode(state, cur_fs);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    g_enumerate_cost = duration.count();
    Statistic(state);

    return state.embedding_cnt_;
}

void Enumerator::Statistic(EnumerateState& state) {
    std::cout << "state_cnt: " << state.state_cnt_ << std::endl;
    std::cout << "fail_state_cnt: " << state.fail_state_cnt_ << std::endl;
    std::cout << "fail rate: " << (double)state.fail_state_cnt_ / state.state_cnt_ << std::endl;
    std::cout << "conflict count: " << conflict_count_ << std::endl;
    std::cout << "empty count: " << empty_count_ << std::endl;
    std::cout << "calc_encode_count: " << calc_encode_count_ << std::endl;
    std::cout << "check_vis_count: " << check_vis_count_ << std::endl;
    std::cout << "get_local_candidate_count: " << get_local_candidate_count_ << std::endl;

    for (int i = 0; i < rq_.nodes_.size(); ++i) {
        if (state.intersection_cost_[i] > 0)
            std::cout << "intersection_cost[" << i << ", rw: " << aux_.rw_order_[i] << "]: " << state.intersection_cost_[i] << std::endl;
    }
    for (int i = 0; i < aux_.query_graph_->getVerticesCount(); ++i) {
        if (state.state_cnt1_[i] > 0) {
            std::cout << "state_cnt[" << i << "]: " << state.state_cnt1_[i] << std::endl;
        }
    }
    for (int i = 0; i < rq_.nodes_.size(); ++i) {
        if (state.fail_state_cnt1_[i] > 0) {
            std::cout << "fail_state_cnt[" << i << "," << aux_.rw_order_[i] << "]: " << state.fail_state_cnt1_[i] << std::endl;
        }
    }
}

void Enumerator::Initialize(EnumerateState& state) {
    intersection_bs_ = std::make_unique<MyBitSet>(MAX_EMBEDDING_NUM);
    lcs_.resize(rq_.nodes_.size());
    for (auto& lc : lcs_) {
        lc.reserve(MAX_EMBEDDING_NUM);
    }
    tmp_lcs_.resize(rq_.nodes_.size());
    for (auto& tmp_lc : tmp_lcs_) {
        tmp_lc.reserve(MAX_EMBEDDING_NUM);
    }
    check_edge_bs_ = std::make_unique<MyBitSet>(aux_.data_graph_->getVerticesCount());

    state.state_cnt_ = state.fail_state_cnt_ = 0;
    state.embedding_cnt_ = state.cur_depth_ = state.cur_offset_ = 0;
    state.finished_ = false;
    state.vis_.resize(aux_.data_graph_->getVerticesCount(), -1);
    state.vis1_.reset(new SparseMap(aux_.data_graph_->getVerticesCount()));
    state.idx_embedding_.resize(aux_.query_graph_->getVerticesCount(), 0);
    state.sdn_embedding_ids_.resize(rq_.nodes_.size(), 0);
    state.candidate_buffers_.resize(aux_.query_graph_->getVerticesCount());
    state.candidate_temps_.resize(aux_.query_graph_->getVerticesCount());
    state.candidate_buffer_size_.resize(aux_.query_graph_->getVerticesCount(), 0);
    state.embedding_.reset(new unsigned[aux_.query_graph_->getVerticesCount()]);
    memset(state.embedding_.get(), -1, sizeof(unsigned) * aux_.query_graph_->getVerticesCount());
    state.product_ = 1;
    for (int i = 0; i < aux_.query_graph_->getVerticesCount(); ++i) {
        state.candidate_buffers_[i].reset(new unsigned[aux_.storage_->get_num_candidates(i)]);
        state.candidate_temps_[i].reset(new unsigned[aux_.storage_->get_num_candidates(i)]);
    }
    state.intersection_cost_.resize(rq_.nodes_.size(), 0);
    state.state_cnt1_.resize(aux_.query_graph_->getVerticesCount(), 0);
    state.fail_state_cnt1_.resize(aux_.query_graph_->getVerticesCount(), 0);
    // failing set related
    state.anc_.resize(aux_.query_graph_->getVerticesCount(), 0);
    state.fs_.resize(aux_.query_graph_->getVerticesCount(), 0);
    for (int i = 0; i < aux_.rw_order_.size(); ++i) {
        auto rw_node_id = aux_.rw_order_[i];
        auto rw_node = rq_.nodes_[rw_node_id];
        if (rw_node->type_ == RewriteQuery::Type::NORMAL) {
            auto normal_node = rw_node;
            auto query_node = normal_node->node_id_;
            for (auto pa_query_node : aux_.parent_query_nodes_[query_node]) {
                state.anc_[query_node] |= state.anc_[pa_query_node];
            }
            BIT_SET(state.anc_[query_node], query_node);
        } else {
            auto mat_node = rw_node;
            unsigned ancestor2 = 0;
            for (int j = 0; j < mat_node->GetInnerOrder().size(); ++j) {
                auto query_node = mat_node->GetInnerOrder()[j];
                for (auto pa_query_node : aux_.parent_query_nodes_[query_node]) {
                    ancestor2 |= state.anc_[pa_query_node];
                }
            }
            for (int j = 0; j < mat_node->GetInnerOrder().size(); ++j) {
                state.anc_[mat_node->GetInnerOrder()[j]] = ancestor2;
                BIT_SET(state.anc_[mat_node->GetInnerOrder()[j]], mat_node->GetInnerOrder()[j]);
            }
        }

        // for (int j = 0; j < rq_.nodes_[rw_node_id]->GetNodeNum(); ++j) {
        //     std::cout << "anc[" << rq_.nodes_[rw_node_id]->GetInnerOrder()[j] << "] = ";
        //     for (int k = 0; k < aux_.query_graph_->getVerticesCount(); ++k) {
        //         if (BIT_TEST(state.anc_[rq_.nodes_[rw_node_id]->GetInnerOrder()[j]], k)) {
        //             std::cout << k << " ";
        //         }
        //     }
        //     std::cout << std::endl;
        // }
    }

    state.trie_bufs_.resize(aux_.query_graph_->getVerticesCount());
    for (int i = 0; i < aux_.query_graph_->getVerticesCount(); ++i) {
        state.trie_bufs_[i].reset(new unsigned[aux_.data_graph_->getVerticesCount()]);
    }

    aux_.tries1_.resize(rq_.nodes_.size());
    for (auto i : aux_.rw_order_) {
        if (rq_.nodes_[i]->type_ == RewriteQuery::Type::MATERIALIZED) {
            if (aux_.enum_methods_[i] == EnumMethod::NormalNode) continue;
            auto mat_node = rq_.nodes_[i];
            auto sqn = mat_node->sqn_;
            auto sdn = mat_node->sdn_;
            mat_node->memory_pool_.reset(new int[sqn->GetEmbeddingWidth() * sdn->GetEmbeddingNum()]);
            memset(mat_node->memory_pool_.get(), -1, sizeof(int) * sqn->GetEmbeddingWidth() * sdn->GetEmbeddingNum());
            mat_node->encoded_embeddings_.resize(sdn->GetEmbeddingNum(), nullptr);
            mat_node->patch_edge_passed_.resize(sdn->GetEmbeddingNum());

            for (int j = 0; j < sdn->GetEmbeddingNum(); ++j) {
                mat_node->encoded_embeddings_[j] = mat_node->memory_pool_.get() + j * sqn->GetEmbeddingWidth();
                mat_node->patch_edge_passed_[j] = 0;
            }

            aux_.tries1_[i] = sdn->trie_.get();
        }
    }
}

long double Enumerator::ExecuteWithinTimeLimit(Graph* query_graph, Graph* data_graph, long long num_limit, unsigned time_limit, unsigned* embeddings) {
    g_exit = false;
    std::future<long double> future = std::async(std::launch::async, [&]() {
        return EnumerateFromRewriteQuery(query_graph, data_graph, num_limit, embeddings);
    });

    std::future_status status;
    do {
        status = future.wait_for(std::chrono::seconds(time_limit));
        if (status == std::future_status::deferred) {
            std::cout << "Deferred\n";
            exit(-1);
        } else if (status == std::future_status::timeout) {
            g_exit = true;
        }
    } while (status != std::future_status::ready);
    return future.get();
}