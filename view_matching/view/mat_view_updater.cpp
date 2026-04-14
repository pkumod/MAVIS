#include "mat_view_updater.h"

#include "MyUtils.h"
#include "subgraph_filter.h"

int MatViewUpdater::TransEmbeddingId(SuperDataNode& src_sdn, SuperDataNode& dst_sdn, int embedding_id) {
    auto embedding = src_sdn.GetEmbedding(embedding_id);
    return dst_sdn.GetEmbeddingId(embedding);
}

void MatViewUpdater::AddGroupEdge(unsigned u_gid, unsigned v_gid, SuperDataEdge& sde, int direction) {
    if (sde.materialized_[direction][u_gid] == false) return;
    auto& linked_groups = sde.linked_groups_[direction][u_gid];
    if (std::find(linked_groups.begin(), linked_groups.end(), v_gid) == linked_groups.end()) {
        linked_groups.push_back(v_gid);
    }
}

void MatViewUpdater::BatchAddGroupEdge(std::unordered_map<ui, std::vector<ui>>& group_edges, SuperDataEdge& sde, int direction) {
    for (auto& [u_gid, v_gids] : group_edges) {
        if (sde.materialized_[direction][u_gid] == false) continue;
        auto& linked_groups = sde.linked_groups_[direction][u_gid];
        flag_cnt_++;
        for (auto ori_gid : linked_groups) {
            flag_[ori_gid] = flag_cnt_;
        }
        for (auto& v_gid : v_gids) {
            if (flag_[v_gid] != flag_cnt_) {
                linked_groups.push_back(v_gid);
                flag_[v_gid] = flag_cnt_;
            }
        }
    }
}

void MatViewUpdater::BuildFilterResultFromEdge(CandidateSpace& cs, unsigned du, unsigned dv, bool record_matched_edge) {
    if (record_matched_edge) {
        matched_edges_.clear();
    }

    for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
        auto qu = i;
        if (query_graph_->getVertexLabel(qu) != data_graph_->getVertexLabel(du)) continue;
        unsigned len;
        auto neighbors = query_graph_->getVertexNeighbors(qu, len);
        for (unsigned j = 0; j < len; ++j) {
            auto qv = neighbors[j];
            if (query_graph_->getVertexLabel(qv) != data_graph_->getVertexLabel(dv)) continue;
            if (record_matched_edge) {
                if (!mat_view_.cs_.CheckEdgeExistence(qu, qv, du, dv)) continue;
            }
            std::vector<std::vector<unsigned>> tmp_res(query_graph_->getVerticesCount());
            bool ret = filter_->FilterFromEdge(tmp_res, qu, qv, du, dv);
            if (ret) {
                // merge tmp_res into res
                if (cs.vcand_.empty()) {
                    cs.vcand_ = std::move(tmp_res);
                } else {
                    for (int k = 0; k < query_graph_->getVerticesCount(); ++k) {
                        std::vector<unsigned> merged;
                        TwoListsMerge(cs.vcand_[k], tmp_res[k], merged);
                        cs.vcand_[k] = std::move(merged);
                    }
                }
                if (record_matched_edge) {
                    matched_edges_.emplace_back(qu, qv);
                }
            }
        }
    }
    if (cs.vcand_.empty() == false) {
        filter_->LinkEdge(cs);
    }
}

void MatViewUpdater::BuildFilterResultFromEdgeBatch(CandidateSpace& cs, std::vector<std::pair<ui, ui>>& edge_list) {
    for (int qu = 0; qu < query_graph_->getVerticesCount(); ++qu) {
        unsigned len;
        auto neighbors = query_graph_->getVertexNeighbors(qu, len);
        for (unsigned j = 0; j < len; ++j) {
            auto qv = neighbors[j];

            for (auto [du, dv] : edge_list) {
                if (query_graph_->getVertexLabel(qu) != data_graph_->getVertexLabel(du) ||
                    query_graph_->getVertexLabel(qv) != data_graph_->getVertexLabel(dv)) {
                    continue;
                }
                std::vector<std::vector<unsigned>> tmp_res(query_graph_->getVerticesCount());
                bool ret = filter_->FilterFromEdge(tmp_res, qu, qv, du, dv);
                if (ret) {
                    // merge tmp_res into res
                    if (cs.vcand_.empty()) {
                        cs.vcand_ = std::move(tmp_res);
                    } else {
                        for (int k = 0; k < query_graph_->getVerticesCount(); ++k) {
                            std::vector<unsigned> merged;
                            MergeTwoSortedLists(cs.vcand_[k], tmp_res[k], merged);
                            cs.vcand_[k] = std::move(merged);
                        }
                    }
                }
            }
        }
    }

    // std::cout << "cs: " << cs.vcand_ << std::endl;

    if (cs.vcand_.empty() == false) {
        filter_->LinkEdge(cs);
    }
}

void MatViewUpdater::BatchGetEmbeddingIds(
    int cur_layer,
    int cur_node_id,
    SuperDataNode& dst_sdn, SuperDataNode& ori_sdn,
    std::vector<int>& embedding_ids, int l, int r) {
    auto& cur_node = ori_sdn.trie_->pool_[cur_node_id];
    if (cur_layer == ori_sdn.embedding_width_ - 1) {
        for (int i = l; i < r; ++i) {
            auto embedding = dst_sdn.GetEmbedding(i);
            if (cur_node.children_.find(embedding[cur_layer]) != cur_node.children_.end()) {
                auto child_id = cur_node.children_[embedding[cur_layer]];
                auto& child_node = ori_sdn.trie_->pool_[child_id];
                embedding_ids[i] = child_node.row_id_;
            }
        }
        return;
    }

    int new_l = l;
    for (int i = l + 1; i <= r; ++i) {
        auto l_embedding = dst_sdn.GetEmbedding(new_l);
        auto embedding = dst_sdn.GetEmbedding(i);
        if (l_embedding[cur_layer] != embedding[cur_layer] || (i == r)) {
            // go to child
            if (cur_node.children_.find(l_embedding[cur_layer]) != cur_node.children_.end()) {
                auto child_id = cur_node.children_[l_embedding[cur_layer]];
                BatchGetEmbeddingIds(
                    cur_layer + 1,
                    child_id,
                    dst_sdn, ori_sdn,
                    embedding_ids, new_l, i);
            }
            new_l = i;
        }
    }
}

void MatViewUpdater::InsertFromCS(CandidateSpace& cs, Graph* data_graph) {
    // if mat_view_ is empty, build from scratch
    if (mat_view_.sdns_.empty()) {
        MatViewBuilder builder;
        builder.Build(query_graph_, data_graph, mat_view_);
        return;
    }

    assert(mat_view_.sqg_.query_graph_ != nullptr);

    // build delta mat view from cs
    std::unique_ptr<SubgraphEstimator> estimator = std::make_unique<SubgraphEstimator>(query_graph_, data_graph, cs);
    MatView delta_mv;
    MatViewBuilder builder;
    bool ret = builder.Materialize(query_graph_, data_graph, estimator.get(), mat_view_.sqg_, cs, delta_mv, true);
    mat_view_.sqg_ = std::move(delta_mv.sqg_);  // sqg was moved for delta mat view, so we need to take it back
    if (!ret) {
        if (builder.oversized_) {
            // rebuild from scratch
            MatViewBuilder full_builder;
            full_builder.Build(query_graph_, data_graph, mat_view_);
            rebuild_cnt_++;
        }
        return;
    }
    complete_mat_view_cnt_++;

    // update candidatespace
    SubgraphFilter::Merge(cs, mat_view_.cs_, query_graph_);

    // std::cout << "updated cs: " << mat_view_.cs_.vcand_ << std::endl;

    // update mat_view_ with delta mat view
    // insert new embeddings to super_nodes
    auto& sqg = mat_view_.sqg_;

    auto super_node_num = sqg.sqns_.size();

    // get id for old embedding and insert new embedding into super-data-node
    std::vector<std::vector<int>> new_embedding_id_sets;
    new_embedding_id_sets.resize(super_node_num);
    for (int i = 0; i < super_node_num; ++i) {
        auto& delta_sdn = delta_mv.sdns_[i];
        auto& origin_sdn = mat_view_.sdns_[i];
        auto& embedding_set = new_embedding_id_sets[i];
        embedding_set.resize(delta_sdn.GetEmbeddingNum(), -1);
        BatchGetEmbeddingIds(0, origin_sdn.trie_->GetRootId(), delta_sdn, origin_sdn, embedding_set, 0, delta_sdn.GetEmbeddingNum());
        for (int j = 0; j < delta_sdn.GetEmbeddingNum(); ++j) {
            if (embedding_set[j] != -1) continue;
            auto embedding = delta_sdn.GetEmbedding(j);
            int ret_id = origin_sdn.InsertEmbedding(embedding);
            embedding_set[j] = ret_id;
        }
    }

    // update index of super data edges
    for (int su = 0; su < super_node_num; ++su) {
        for (auto edge : sqg.adj_[su]) {
            auto sv = edge.dst;
            if (su >= sv) continue;
            auto& ses = mat_view_.ses_[su][sv];
            // insert embedding into index of super_data_edge
            auto& delta_sdn_u = delta_mv.sdns_[su];
            auto& delta_sdn_v = delta_mv.sdns_[sv];
            auto& sdn_u = mat_view_.sdns_[su];
            auto& sdn_v = mat_view_.sdns_[sv];
            for (int i = 0; i < delta_sdn_u.GetEmbeddingNum(); ++i) {
                auto embedding_id = new_embedding_id_sets[su][i];
                assert(embedding_id != -1);
                if (ses.CheckEmbedding(embedding_id, 0) == false) {
                    ses.InsertEmbedding(sdn_u, sqg.sqns_[su], edge.src_set, embedding_id, 0);
                }
            }
            for (int i = 0; i < delta_sdn_v.GetEmbeddingNum(); ++i) {
                auto embedding_id = new_embedding_id_sets[sv][i];
                assert(embedding_id != -1);
                if (ses.CheckEmbedding(embedding_id, 1) == false) {
                    ses.InsertEmbedding(sdn_v, sqg.sqns_[sv], edge.dst_set, embedding_id, 1);
                }
            }

            // extract all super edge candidates from delta_mat_view
            auto& delta_ses = delta_mv.ses_[su][sv];
            std::vector<std::pair<ui, ui>> candidates;
            for (int i = 0; i < delta_sdn_u.GetEmbeddingNum(); ++i) {
                auto gid = delta_ses.su_index_.embedding_id2group_id_[i];
                for (auto& link_gid : delta_ses.linked_groups_[0][gid]) {
                    for (auto& dst : delta_ses.sv_index_.group_id2embedding_ids_[link_gid]) {
                        candidates.emplace_back(i, dst);
                    }
                }
            }

            // insert candidates into super_data_edge
            std::unordered_map<ui, std::vector<ui>> group_edges[2];
            for (auto& candidate : candidates) {
                auto u = new_embedding_id_sets[su][candidate.first];
                auto v = new_embedding_id_sets[sv][candidate.second];
                assert(u != -1 && v != -1);
                auto u_gid = ses.su_index_.embedding_id2group_id_[u];
                auto v_gid = ses.sv_index_.embedding_id2group_id_[v];
                group_edges[0][u_gid].push_back(v_gid);
                group_edges[1][v_gid].push_back(u_gid);
            }
            BatchAddGroupEdge(group_edges[0], ses, 0);
            BatchAddGroupEdge(group_edges[1], ses, 1);
        }
    }
}

void MatViewUpdater::InsertEdgeBatch(std::vector<std::pair<ui, ui>>& edges, Graph* data_graph) {
    CandidateSpace cs;
    BuildFilterResultFromEdgeBatch(cs, edges);
    if (cs.vcand_.empty()) return;
    complete_cs_cnt_++;
    InsertFromCS(cs, data_graph);
}

void MatViewUpdater::InsertEdge(ui u, ui v, Graph* data_graph) {
    // build filter result for the new edge (u, v)
    CandidateSpace cs;
    BuildFilterResultFromEdge(cs, u, v);
    if (cs.vcand_.empty()) return;
    complete_cs_cnt_++;

    InsertFromCS(cs, data_graph);
}

bool MatViewUpdater::CheckEmbedding(ui su, ui embedding_id) {
    auto& sqg = mat_view_.sqg_;
    for (auto item : sqg.adj_[su]) {
        auto sv = item.dst;
        auto ses = mat_view_.GetSuperDataEdgeView(su, sv);
        auto gid = ses.su_index_.embedding_id2group_id_[embedding_id];
        if (ses.materialized_[gid] == false) continue;
        if (ses.linked_groups_[gid].empty()) return false;
    }
    return true;
}

void MatViewUpdater::CascadeDelete(ui su, ui embedding_id) {

    // save neighbor embedding
    std::vector<std::pair<ui, ui>> neighbor_embeddings;  // (sv, embedding_id)
    for (auto item : mat_view_.sqg_.adj_[su]) {
        auto sv = item.dst;
        auto ses = mat_view_.GetSuperDataEdgeView(su, sv);
        auto gid = ses.su_index_.embedding_id2group_id_[embedding_id];
        if (ses.materialized_[gid] == false) continue;
        for (auto v_gid : ses.linked_groups_[gid]) {
            auto& sv_index = ses.sv_index_;
            for (auto neighbor_embedding_id : sv_index.group_id2embedding_ids_[v_gid]) {
                neighbor_embeddings.emplace_back(sv, neighbor_embedding_id);
            }
        }
    }

    // delete related info
    // delete embedding in super data edge
    for (auto item : mat_view_.sqg_.adj_[su]) {
        auto sv = item.dst;
        auto ses = mat_view_.GetSuperDataEdgeView(su, sv);
        auto gid = ses.su_index_.embedding_id2group_id_[embedding_id];

        // delete embedding in group
        auto& embedding_ids = ses.su_index_.group_id2embedding_ids_[gid];
        EraseValueFromSortedVector(embedding_ids, embedding_id);

        if (embedding_ids.empty()) {  // delete group
            // delete group edge
            if (ses.materialized_[gid]) {
                for (auto v_gid : ses.linked_groups_[gid]) {
                    auto& rev_link = ses.rev_linked_groups_[v_gid];
                    EraseValueFromVector(rev_link, (int)gid);
                }
                ses.linked_groups_[gid].clear();
                ses.materialized_[gid] = false;
            } else {
                for (ui v_gid = 0; v_gid < ses.sv_index_.group_id2embedding_ids_.size(); ++v_gid) {
                    auto& rev_link = ses.rev_linked_groups_[v_gid];
                    if (std::binary_search(rev_link.begin(), rev_link.end(), (int)gid)) {
                        EraseValueFromVector(rev_link, (int)gid);
                    }
                }
            }

            // delete group in su_index
            ses.su_index_.embedding_id2group_id_[embedding_id] = -1;  // mark as deleted
            ses.su_index_.group_id2embedding_ids_[gid].clear();
            auto embedding = mat_view_.sdns_[su].GetEmbedding(embedding_id);
            std::vector<int> port_vertice_poses;
            std::vector<int> key;
            for (auto v : item.src_set) {
                port_vertice_poses.push_back(mat_view_.sqg_.sqns_[su].pos_.at(v));
            }
            GetKeyByPos(embedding, port_vertice_poses, key);
            ses.su_index_.key2group_id_.erase(key);
        }
    }
    // delete embedding in super data node
    mat_view_.sdns_[su].DeleteEmbedding(embedding_id);

    // check and delete neighbor embeddings
    for (auto [sid, embedding_id] : neighbor_embeddings) {
        if (CheckEmbedding(sid, embedding_id) == false) {
            CascadeDelete(sid, embedding_id);
        }
    }
}

void MatViewUpdater::DeleteSuperDataEdge(ui su, ui sv, ui im_u_gid, ui im_v_gid, MatView& imat_view) {
    auto ses = mat_view_.GetSuperDataEdgeView(su, sv);
    auto im_ses = imat_view.GetSuperDataEdgeView(su, sv);

    auto u_gid = 0, v_gid = 0;
    {
        auto im_first_embedding_id = im_ses.su_index_.group_id2embedding_ids_[im_u_gid][0];
        auto first_embedding_id = TransEmbeddingId(imat_view.sdns_[su], mat_view_.sdns_[su], im_first_embedding_id);
        u_gid = ses.su_index_.embedding_id2group_id_[first_embedding_id];
    }
    {
        auto im_first_embedding_id = im_ses.sv_index_.group_id2embedding_ids_[im_v_gid][0];
        auto first_embedding_id = TransEmbeddingId(imat_view.sdns_[sv], mat_view_.sdns_[sv], im_first_embedding_id);
        v_gid = ses.sv_index_.embedding_id2group_id_[first_embedding_id];
    }

    for (int i = 0; i < 2; ++i) {
        auto& lg = mat_view_.GetSuperDataEdgeView(su, sv).linked_groups_[u_gid];
        if (std::find(lg.begin(), lg.end(), v_gid) != lg.end()) {
            EraseValueFromVector(lg, (int)v_gid);
        }

        std::swap(su, sv);
        std::swap(u_gid, v_gid);
    }
}

void MatViewUpdater::DeleteEdge(VertexID u, VertexID v, Graph* data_graph) {
    if (mat_view_.sdns_.empty()) return;

    auto& sqg = mat_view_.sqg_;

    for (int qu = 0; qu < query_graph_->getVerticesCount(); ++qu) {
        if (query_graph_->getVertexLabel(qu) != data_graph_->getVertexLabel(u)) continue;

        unsigned len;
        auto neighbors = query_graph_->getVertexNeighbors(qu, len);
        for (unsigned j = 0; j < len; ++j) {
            auto qv = neighbors[j];
            if (query_graph_->getVertexLabel(qv) != data_graph_->getVertexLabel(v)) continue;

            SubgraphFilter::Remove(mat_view_.cs_, query_graph_, qu, qv, u, v);

            auto qu_sid = mat_view_.sqg_.GetId2SQNId(qu);
            auto qv_sid = mat_view_.sqg_.GetId2SQNId(qv);
            if (qu_sid == qv_sid) {  // broken edge appears in a candidate of super node
                auto sid = qu_sid;
                auto qu_pos = sqg.sqns_[sid].pos_.at(qu);
                auto qv_pos = sqg.sqns_[sid].pos_.at(qv);
                auto& sdn = mat_view_.sdns_[sid];
                for (int embedding_id = 0; embedding_id < sdn.GetEmbeddingNum(); ++embedding_id) {
                    auto embedding = sdn.GetEmbedding(embedding_id);
                    if (embedding[qu_pos] == u && embedding[qv_pos] == v) {
                        CascadeDelete(sid, embedding_id);
                    }
                }
            } else {
                // qu and qv are in different super nodes, and are port vertices
                auto qu_pos = sqg.sqns_[qu_sid].pos_.at(qu);
                auto qv_pos = sqg.sqns_[qv_sid].pos_.at(qv);
                auto& sdn_u = mat_view_.sdns_[qu_sid];
                auto ses = mat_view_.GetSuperDataEdgeView(qu_sid, qv_sid);
                auto& su_index = ses.su_index_;
                for (int su_gid = 0; su_gid < su_index.group_id2embedding_ids_.size(); ++su_gid) {
                    auto first_embedding = sdn_u.GetEmbedding(su_index.group_id2embedding_ids_[su_gid][0]);
                    if (first_embedding[qu_pos] != u) continue;  // skip the group that
                    // for (auto sv_gid : ses.linked_groups_[su_gid])
                    for (int i = 0; i < ses.linked_groups_[su_gid].size(); ++i) {
                        auto sv_gid = ses.linked_groups_[su_gid][i];
                        auto& sv_index = ses.sv_index_;
                        auto& sdn_v = mat_view_.sdns_[qv_sid];

                        auto first_embedding_v = sdn_v.GetEmbedding(sv_index.group_id2embedding_ids_[sv_gid][0]);
                        if (first_embedding_v[qv_pos] != v) continue;

                        // delete super data edge
                        EraseValueFromVector(ses.linked_groups_[su_gid], (int)sv_gid);
                        i--;
                        EraseValueFromVector(ses.rev_linked_groups_[sv_gid], (int)su_gid);

                        // check if the embedding, which in group su_gid and sv_gid, should be deleted
                        auto su_embeddings = su_index.group_id2embedding_ids_[su_gid];
                        for (auto embedding_id : su_embeddings) {
                            if (CheckEmbedding(qu_sid, embedding_id) == false) {
                                CascadeDelete(qu_sid, embedding_id);
                            }
                        }
                        auto sv_embeddings = sv_index.group_id2embedding_ids_[sv_gid];
                        for (auto embedding_id : sv_embeddings) {
                            if (CheckEmbedding(qv_sid, embedding_id) == false) {
                                CascadeDelete(qv_sid, embedding_id);
                            }
                        }
                    }
                }
            }
        }
    }
}

void PrintMatView(MatView& mv, SuperQueryGraph& sqg) {
    for (int i = 0; i < sqg.sqns_.size(); ++i) {
        auto& sqn = sqg.sqns_[i];
        auto& sdn = mv.sdns_[i];
        std::cout << "super node " << i << ": " << sdn.GetEmbeddingNum() << " embeddings" << std::endl;
        std::cout << "order: " << sqn.order_ << std::endl;
        for (int j = 0; j < sdn.GetEmbeddingNum(); ++j) {
            auto embedding = sdn.GetEmbedding(j);
            for (int k = 0; k < sdn.embedding_width_; ++k) {
                std::cout << embedding[k] << " ";
            }
            std::cout << std::endl;
        }
    }
}

void MatViewUpdater::DeleteEdgeBatch(std::vector<std::pair<ui, ui>>& edges, Graph* data_graph) {
    if (mat_view_.sdns_.empty()) return;

    auto& sqg = mat_view_.sqg_;

    for (int qu = 0; qu < query_graph_->getVerticesCount(); ++qu) {
        unsigned len;
        auto neighbors = query_graph_->getVertexNeighbors(qu, len);
        for (unsigned j = 0; j < len; ++j) {
            auto qv = neighbors[j];

            std::vector<std::pair<ui, ui>> filtered_edges;
            for (auto [u, v] : edges) {
                if (query_graph_->getVertexLabel(qu) != data_graph_->getVertexLabel(u) ||
                    query_graph_->getVertexLabel(qv) != data_graph_->getVertexLabel(v)) {
                    continue;
                }
                filtered_edges.emplace_back(u, v);
                SubgraphFilter::Remove(mat_view_.cs_, query_graph_, qu, qv, u, v);
            }

            if (filtered_edges.empty()) continue;

            std::sort(filtered_edges.begin(), filtered_edges.end());

            auto qu_sid = mat_view_.sqg_.GetId2SQNId(qu);
            auto qv_sid = mat_view_.sqg_.GetId2SQNId(qv);
            if (qu_sid == qv_sid) {  // broken edge appears in a candidate of super node
                auto sid = qu_sid;
                auto qu_pos = sqg.sqns_[sid].pos_.at(qu);
                auto qv_pos = sqg.sqns_[sid].pos_.at(qv);
                auto& sdn = mat_view_.sdns_[sid];
                for (int embedding_id = 0; embedding_id < sdn.GetEmbeddingNum(); ++embedding_id) {
                    auto embedding = sdn.GetEmbedding(embedding_id);
                    // if (embedding[qu_pos] == u && embedding[qv_pos] == v) {
                    //     CascadeDelete(sid, embedding_id);
                    // }
                    // if (filtered_edges.count({embedding[qu_pos], embedding[qv_pos]}) > 0) {
                    //     CascadeDelete(sid, embedding_id);
                    // }
                    auto edge = std::make_pair(embedding[qu_pos], embedding[qv_pos]);
                    if (std::binary_search(filtered_edges.begin(), filtered_edges.end(), edge)) {
                        CascadeDelete(sid, embedding_id);
                    }
                }
            } else {
                // qu and qv are in different super nodes, and are port vertices
                auto qu_pos = sqg.sqns_[qu_sid].pos_.at(qu);
                auto qv_pos = sqg.sqns_[qv_sid].pos_.at(qv);
                auto& sdn_u = mat_view_.sdns_[qu_sid];
                auto ses = mat_view_.GetSuperDataEdgeView(qu_sid, qv_sid);
                auto& su_index = ses.su_index_;
                for (int su_gid = 0; su_gid < su_index.group_id2embedding_ids_.size(); ++su_gid) {
                    auto first_embedding = sdn_u.GetEmbedding(su_index.group_id2embedding_ids_[su_gid][0]);

                    auto edge = std::make_pair(first_embedding[qu_pos], (unsigned)0);
                    auto lower = std::lower_bound(filtered_edges.begin(), filtered_edges.end(), edge);
                    if (lower == filtered_edges.end() || lower->first != first_embedding[qu_pos]) {
                        continue;
                    }
                    
                    for (int i = 0; i < ses.linked_groups_[su_gid].size(); ++i) {
                        auto sv_gid = ses.linked_groups_[su_gid][i];
                        auto& sv_index = ses.sv_index_;
                        auto& sdn_v = mat_view_.sdns_[qv_sid];

                        auto first_embedding_v = sdn_v.GetEmbedding(sv_index.group_id2embedding_ids_[sv_gid][0]);
                        
                        auto edge = std::make_pair(first_embedding[qu_pos], first_embedding_v[qv_pos]);
                        if (std::binary_search(filtered_edges.begin(), filtered_edges.end(), edge) == false) continue;

                        // delete super data edge
                        EraseValueFromVector(ses.linked_groups_[su_gid], (int)sv_gid);
                        i--;
                        EraseValueFromVector(ses.rev_linked_groups_[sv_gid], (int)su_gid);

                        // check if the embedding, which in group su_gid and sv_gid, should be deleted
                        auto su_embeddings = su_index.group_id2embedding_ids_[su_gid];
                        for (auto embedding_id : su_embeddings) {
                            if (CheckEmbedding(qu_sid, embedding_id) == false) {
                                CascadeDelete(qu_sid, embedding_id);
                            }
                        }

                        auto sv_embeddings = sv_index.group_id2embedding_ids_[sv_gid];
                        for (auto embedding_id : sv_embeddings) {
                            if (CheckEmbedding(qv_sid, embedding_id) == false) {
                                CascadeDelete(qv_sid, embedding_id);
                            }
                        }
                    }
                }
            }
        }
    }
}

void MatViewUpdater::Statistics() {
    std::cout << "Total complete candidate space built: " << complete_cs_cnt_ << std::endl;
    std::cout << "Total complete mat view built: " << complete_mat_view_cnt_ << std::endl;
    std::cout << "Total rebuild from scratch: " << rebuild_cnt_ << std::endl;
}