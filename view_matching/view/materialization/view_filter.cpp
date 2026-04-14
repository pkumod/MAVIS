#include "view_filter.h"
#include "global_vars.h"
#include "MyUtils.h"

void ViewFilter::CalcNewNodeIds(unsigned su, std::vector<int>& new_node_ids) {
    auto& sdn = sdns_[su];
    int sum = 0;
    for (int i = 0; i < sdn.GetEmbeddingNum(); ++i) {
        if (sdn.complete_[i]) {
            new_node_ids.push_back(sum++);
        } else {
            new_node_ids.push_back(-1);
        }
    }
}

void ViewFilter::UpdateIndex(unsigned su, unsigned sv, const std::vector<int>& new_node_ids, std::vector<int>& new_group_ids) {
    bool d = su > sv;  // 0: su --> sv, 1: sv --> su
    auto& se = d ? ses_[sv][su] : ses_[su][sv];
    auto& index = d ? se.sv_index_ : se.su_index_;
    int group_n = index.group_id2embedding_ids_.size();
    int sum = 0;
    // get new_group_ids
    for (int i = 0; i < group_n; ++i) {
        bool empty = true;
        for (auto embedding_id : index.group_id2embedding_ids_[i]) {
            if (new_node_ids[embedding_id] != -1) {
                empty = false;
                break;
            }
        }
        new_group_ids.push_back(empty ? -1 : sum++);
    }
    // update embedding_id2group_id_
    std::vector<unsigned > new_embedding_id2group_id;
    for (int i = 0; i < index.embedding_id2group_id_.size(); ++i) {
        if (new_node_ids[i] != -1) {
            new_embedding_id2group_id.push_back(new_group_ids[index.embedding_id2group_id_[i]]);
        }
    }
    index.embedding_id2group_id_ = std::move(new_embedding_id2group_id);
    // update group_id2embedding_ids_
    std::vector<std::vector<unsigned >> new_group_id2embedding_ids;
    new_group_id2embedding_ids.resize(sum);
    for (int i = 0; i < index.embedding_id2group_id_.size(); ++i) {
        new_group_id2embedding_ids[index.embedding_id2group_id_[i]].push_back(i);
    }
    index.group_id2embedding_ids_ = std::move(new_group_id2embedding_ids);
}

void ViewFilter::UpdateLink(unsigned su, unsigned sv, std::vector<int>& su_new_group_ids, std::vector<int>& sv_new_group_ids) {
    auto d = su > sv;  // 0: su --> sv, 1: sv --> su
    auto& se = d ? ses_[sv][su] : ses_[su][sv];
    auto& src_index = d ? se.sv_index_ : se.su_index_;
    auto& dst_index = d ? se.su_index_ : se.sv_index_;
    auto& link = se.linked_groups_[d];
    auto& materialized = se.materialized_[d];
    auto& all_materialized = se.all_materialized_[d];
    std::vector<std::vector<int>> new_link;
    std::vector<bool> new_materialized;
    new_link.resize(src_index.group_id2embedding_ids_.size());
    new_materialized.resize(src_index.group_id2embedding_ids_.size(), false);
    all_materialized = true;
    for (int i = 0; i < link.size(); ++i) {
        if (su_new_group_ids[i] == -1) continue;
        new_materialized[su_new_group_ids[i]] = materialized[i];
        if (!materialized[i]) {
            all_materialized = false;
        }
        for (auto j : link[i]) {
            if (sv_new_group_ids[j] != -1) {
                new_link[su_new_group_ids[i]].push_back(sv_new_group_ids[j]);
            }
        }
    }
    link = std::move(new_link);
    materialized = std::move(new_materialized);
}

void ViewFilter::RemoveInvalidEmbeddings(unsigned su, unsigned sv) {
    std::vector<int> su_new_node_ids, sv_new_node_ids;
    std::vector<int> su_new_group_ids, sv_new_group_ids;
    CalcNewNodeIds(su, su_new_node_ids);
    CalcNewNodeIds(sv, sv_new_node_ids);
    UpdateIndex(su, sv, su_new_node_ids, su_new_group_ids);
    UpdateIndex(sv, su, sv_new_node_ids, sv_new_group_ids);
    UpdateLink(su, sv, su_new_group_ids, sv_new_group_ids);
    UpdateLink(sv, su, sv_new_group_ids, su_new_group_ids);
}


void ViewFilter::RemoveInvalidEmbeddings() {
    for (int i = 0; i < sqg_.sqns_.size(); ++i) {
        auto u = i;
        for (int j = 0; j < sqg_.adj_[i].size(); ++j) {
            auto v = sqg_.adj_[i][j].dst;
            if (u < v) {
                RemoveInvalidEmbeddings(i, sqg_.adj_[i][j].dst);
            }
        }
    }
    for (int i = 0; i < sqg_.sqns_.size(); ++i) {
        auto& sdn = sdns_[i];
        ui complete_embedding_num = 0;
        for (ui j = 0; j < sdn.GetEmbeddingNum(); ++j) {
            complete_embedding_num += sdn.complete_[j];
        }
        std::unique_ptr<unsigned[]> new_embedding_pool(new unsigned[complete_embedding_num * sdn.embedding_width_]);
        // copy valid embeddings
        int new_embedding_id = 0;
        for (ui j = 0; j < sdn.GetEmbeddingNum(); ++j) {
            if (sdn.complete_[j]) {
                auto embedding = sdn.GetEmbedding(j);
                memcpy(new_embedding_pool.get() + new_embedding_id * sdn.embedding_width_, embedding, sdn.embedding_width_ * sizeof(unsigned));
                new_embedding_id++;
            }
        }
        sdn.size_ = complete_embedding_num;
        sdn.embedding_pool_.reset(new_embedding_pool.release());
    }
    // release complete_
    for (int i = 0; i < sqg_.sqns_.size(); ++i) sdns_[i].ReleaseComplete();
}

bool ViewFilter::CheckEdgeValid(ui su, ui sv, ui su_ebd_id) {
    auto se = GetSuperDataEdgeView(su, sv, ses_);
    auto su_gid = se.su_index_.embedding_id2group_id_[su_ebd_id];
    if (se.materialized_[su_gid] == false) return true;
    auto su_embedding = sdns_[su].GetEmbedding(su_ebd_id);
    // for (int i = 0; i < sdns_[su].embedding_width_; ++i) {
    //     bs->Set(su_embedding[i]);
    // }
    for (auto sv_gid : se.linked_groups_[su_gid]) {
        for (auto sv_ebd_id : se.sv_index_.group_id2embedding_ids_[sv_gid]) {
            if (sdns_[sv].complete_[sv_ebd_id] == false) continue;
            auto sv_embedding = sdns_[sv].GetEmbedding(sv_ebd_id);
            bool flag = true;
            for (int i = 0; i < sdns_[sv].embedding_width_; ++i) {
                for (int j = 0; j < sdns_[su].embedding_width_; ++j) {
                    check_count_++;
                    if (sv_embedding[i] == su_embedding[j]) {
                        flag = false;
                        break;
                    }
                }
                if (!flag) break;
            }
            if (flag) return true;
            // for (int i = 0; i < sdns_[sv].embedding_width_; ++i) {
            //     // check_count_++;
            //     if (bs->Test(sv_embedding[i])) {
            //         flag = false;
            //         break;
            //     }
            // }
            // if (flag) {
            //     for (int i = 0; i < sdns_[su].embedding_width_; ++i) {
            //         bs->Reset(su_embedding[i]);
            //     }
            //     return true;
            // }
        }
    }
    // for (int i = 0; i < sdns_[su].embedding_width_; ++i) {
    //     bs->Reset(su_embedding[i]);
    // }
    return false;
}

int ViewFilter::FilterBy(unsigned u, unsigned v) {
    // std::cout << "filter by " << u << " " << v << "\n";
    auto se = GetSuperDataEdgeView(u, v, ses_);
    int complete_num = 0;
    for (ui src_ebd_id = 0; src_ebd_id < sdns_[u].GetEmbeddingNum(); ++src_ebd_id) {
        if (sdns_[u].complete_[src_ebd_id] == false) continue;
        auto src_group_id = se.su_index_.embedding_id2group_id_[src_ebd_id];
        if (se.materialized_[src_group_id] == false) {  // unmaterialized
            sdns_[u].complete_[src_ebd_id] = true;
        }
        else {
            sdns_[u].complete_[src_ebd_id] = CheckEdgeValid(u, v, src_ebd_id);
        }
        complete_num += sdns_[u].complete_[src_ebd_id];
    }
    return complete_num;
}

bool ViewFilter::FilterEmbeddings() {
    for (int su = 0; su < sqg_.sqns_.size(); ++su) {
        for (auto adj: sqg_.adj_[su]) {
            ui sv = adj.dst;
            int complete_num = FilterBy(su, sv);
            if (complete_num == 0) return false;
        }
    }

    std::vector<ui> old_count, new_count;
    for (int su = 0; su < sqg_.sqns_.size(); ++su) {
        old_count.push_back(sdns_[su].GetEmbeddingNum());
        int complete_num = 0;
        for (int i = 0; i < sdns_[su].GetEmbeddingNum(); ++i) {
           complete_num += sdns_[su].complete_[i];
        }
        new_count.push_back(complete_num);
    }

    do {
        double max_ratio = 0.0;
        int max_id = -1;
        for (int su = 0; su < sqg_.sqns_.size(); ++su) {
            double ratio = (double)(old_count[su] - new_count[su]) / old_count[su];
            if (ratio > max_ratio) {
                max_ratio = ratio;
                max_id = su;
            }
        }
        if (max_id == -1) break;

        for (auto adj: sqg_.adj_[max_id]) {
            ui sv = adj.dst;
            // std::cout << "Filtering by super query node " << max_id << " to " << sv << "\n";
            // auto start = std::chrono::high_resolution_clock::now();
            // check_count_ = 0;
            int complete_num = FilterBy(sv, max_id);
            // auto end = std::chrono::high_resolution_clock::now();
            // auto filter_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            // std::cout << "  Filter time: " << filter_time << " ms\n";
            // std::cout << "  Check count: " << check_count_ << "\n";
            if (complete_num != new_count[sv]) {
                new_count[sv] = complete_num;
            }
        }

        old_count[max_id] = new_count[max_id];

        // for (int i = 0; i < sqg_.sqns_.size(); ++i) {
        //     std::cout << "  Super Query Node " << i << ": " << new_count[i] << "/" << old_count[i] << " embeddings remain\n";
        // }
    } while (true);

    // std::stringstream ss;
    // ss << "After filtering:\n";
    // for (int su = 0; su < sqg_.sqns_.size(); ++su) {
    //     ui complete_embedding_num = 0;
    //     for (ui j = 0; j < sdns_[su].GetEmbeddingNum(); ++j) {
    //         complete_embedding_num += sdns_[su].complete_[j];
    //     }
    //     ss << "  Super Query Node " << su << ": " << complete_embedding_num << "/" << sdns_[su].GetEmbeddingNum() << " embeddings remain\n";
    // }

    // if (log_str != nullptr) {
    //     *log_str += ss.str();
    // }
    // else {
    //     std::cout << ss.str();
    // }
    return true;
}