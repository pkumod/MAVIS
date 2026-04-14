#include "materializer.h"
#include "edge_materializer.h"
#include "node_materializer.h"
#include "global_vars.h"
#include "view_filter.h"

Materializer::Materializer(Graph* query_graph, Graph* data_graph, SubgraphEstimator* estimator, TreePartitionGraph& sqg, CandidateSpace& cs):
    query_graph_(query_graph), data_graph_(data_graph), estimator_(estimator), sqg_(std::move(sqg)), cs_(cs) {}

void Materializer::OutputGroupEmbeddings(int su, std::vector<ui>& ids) {
    auto& sdn = sdns_[su];
    auto embedding_len = sdn.embedding_width_;
    for (auto id : ids) {
        auto embedding = sdn.GetEmbedding(id);
        std::cout << std::vector<int>(embedding, embedding + embedding_len) << std::endl;
    }
}

void Materializer::OutputAllLinks() {
    for (int i = 0; i < sqg_.sqns_.size(); ++i) {
        auto u = i;
        for (int j = 0; j < sqg_.adj_[i].size(); ++j) {
            auto v = sqg_.adj_[i][j].dst;
            if (u > v) continue;
            auto& se = ses_[u][v];
            for (int src_group_id = 0; src_group_id < se.su_index_.group_id2embedding_ids_.size(); ++src_group_id) {
                for (auto dst_group_id : se.linked_groups_[0][src_group_id]) {
                    std::cout << "src_group_id: " << src_group_id << " dst_group_id: " << dst_group_id << std::endl;
                    assert(src_group_id < se.su_index_.group_id2embedding_ids_.size());
                    assert(dst_group_id < se.sv_index_.group_id2embedding_ids_.size());
                    // OutputGroupEmbeddings(u, se.su_index_.group_id2embedding_ids_[src_group_id]);
                    // OutputGroupEmbeddings(v, se.sv_index_.group_id2embedding_ids_[dst_group_id]);
                }
            }
        }
    }
}

void OutputAllEmbeddings(Materializer* imv) {
    for (int i = 0; i < imv->sqg_.sqns_.size(); ++i) {
        auto& sdn = imv->sdns_[i];
        std::cout << "node " << i << ":\n";
        for (int j = 0; j < sdn.GetEmbeddingNum(); ++j) {
            auto embedding = sdn.GetEmbedding(j);
            std::cout << std::vector<int>(embedding, embedding + sdn.embedding_width_) << std::endl;
        }
    }
}

bool Materializer::CheckCompleteness() {
    for (ui su = 0; su < sqg_.sqns_.size(); ++su) {
        for (ui sdn_id = 0; sdn_id < sdns_[su].GetEmbeddingNum(); ++sdn_id) {
            if (sdns_[su].complete_[sdn_id] == false) continue;
            for (auto adj: sqg_.adj_[su]) {
                ui sv = adj.dst;
                auto se = GetSuperDataEdgeView(su, sv, ses_);
                auto su_gid = se.su_index_.embedding_id2group_id_[sdn_id];
                if (se.materialized_[su_gid] == false) continue;
                bool has_link = false;
                for (auto sv_gid : se.linked_groups_[su_gid]) {
                    for (auto sv_ebd_id : se.sv_index_.group_id2embedding_ids_[sv_gid]) {
                        if (sdns_[sv].complete_[sv_ebd_id] == false) continue;
                        auto su_embedding = sdns_[su].GetEmbedding(sdn_id);
                        auto sv_embedding = sdns_[sv].GetEmbedding(sv_ebd_id);
                        if (!ContainSameValue(su_embedding, sdns_[su].embedding_width_,
                                             sv_embedding, sdns_[sv].embedding_width_)) {
                            has_link = true;
                            break;
                        }
                    }
                    if (has_link) break;
                }
                if (!has_link) {
                    std::cout << "incomplete embedding found: su " << su << " sdn_id " << sdn_id << "\n";
                    return false;
                }
            }
        }
    }
    return true;
}

bool Materializer::Exec() {
    sdns_.resize(sqg_.sqns_.size());
    ses_.resize(sqg_.sqns_.size());
    for (unsigned i = 0; i < sqg_.sqns_.size(); ++i) ses_[i].resize(sqg_.sqns_.size());

    // materialize super data nodes
    NodeMaterializer node_materializer(sdns_, sqg_.sqns_, estimator_, cs_, oversized_);
    auto start = std::chrono::high_resolution_clock::now();
    if (node_materializer.MaterializeSDNs() == false) return false;
    auto end = std::chrono::high_resolution_clock::now();
    materialize_cost_ = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // link super edges
    EdgeMaterializer edge_materializer(sqg_, sdns_, ses_, data_graph_, cs_);
    for (unsigned i = 0; i < sqg_.sqns_.size(); ++i) {
        sdns_[i].complete_.resize(sdns_[i].GetEmbeddingNum(), true);
    }
    start = std::chrono::high_resolution_clock::now();
    edge_materializer.LinkSuperEdge();
    end = std::chrono::high_resolution_clock::now();
    link_superedge_cost_ = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // filter embeddings
    ViewFilter view_filter(sqg_, sdns_, ses_, data_graph_->getVerticesCount());
    start = std::chrono::high_resolution_clock::now();
    bool ret = view_filter.FilterEmbeddings();
    // bool ret = view_filter.FilterEmbeddings1();
    end = std::chrono::high_resolution_clock::now();
    filter_embedding_cost_ = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // if (!CheckCompleteness()) {
    //     std::cout << "incomplete embeddings exist after materialization and linking\n";
    //     exit(1);
    // }

    if (log_str != nullptr) {
        *log_str += "filter cost: " + std::to_string(filter_embedding_cost_) + " us\n";
    }

    if (ret == false) {
        // std::cout << "empty mat view";
    }
    else {
        // std::cout << "finished filter embeddings\n";
        view_filter.RemoveInvalidEmbeddings();
        // std::cout << "finished removing invalid embeddings\n";

        // build key2group_id index for super data edges. This index is used for updations.
        for (unsigned su = 0; su < sqg_.sqns_.size(); ++su) {
            for (auto adj : sqg_.adj_[su]) {
                auto sv = adj.dst;
                if (su >= sv) continue;
                ses_[su][sv].su_index_.BuildKey2GroupId(sdns_[su], sqg_.sqns_[su], adj.src_set);
                ses_[su][sv].sv_index_.BuildKey2GroupId(sdns_[sv], sqg_.sqns_[sv], adj.dst_set);
            }
        }
    }

    // Statistics();

    view_evaluation_cost_ = link_superedge_cost_ + materialize_cost_ + filter_embedding_cost_;

    // std::cout << "*** finished building view ***\n";

    return ret;
}