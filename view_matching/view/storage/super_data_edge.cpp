#include "super_data_edge.h"

unsigned long long SuperDataEdge::Index::MemoryCost() {
    unsigned long long embedding_id2group_id_cost = embedding_id2group_id_.size() * sizeof(unsigned);
    unsigned long long group_id2embedding_ids_cost = 0;
    for (auto& group : group_id2embedding_ids_) {
        group_id2embedding_ids_cost += group.size() * sizeof(unsigned);
    }
    unsigned long long key2group_id_cost = 0;
    for (auto& [key, value] : key2group_id_) {
        key2group_id_cost += key.size() * sizeof(int) + sizeof(unsigned);
    }
    return embedding_id2group_id_cost + group_id2embedding_ids_cost + key2group_id_cost;
}


bool SuperDataEdge::CheckEmbedding(unsigned embedding_id, int d) {
    auto& index = d == 0 ? su_index_ : sv_index_;
    if (index.embedding_id2group_id_.size() > embedding_id && index.embedding_id2group_id_[embedding_id] != -1) {
        return true;
    }
    return false;
}

int SuperDataEdge::AllocateGroupId(int d) {
    auto& index = d == 0 ? su_index_ : sv_index_;
    index.group_id2embedding_ids_.emplace_back();
    linked_groups_[d].emplace_back();
    materialized_[d].emplace_back(true);
    return index.group_id2embedding_ids_.size() - 1;
}

void SuperDataEdge::InsertEmbedding(SuperDataNode& sdn, SuperQueryNode& sqn, std::vector<ui>& port_vertices, unsigned embedding_id, int d) { 
    std::vector<int> key;
    std::vector<int> port_vertice_poses;
    port_vertice_poses.reserve(sqn.GetVarNum());
    for (auto port_vertex : port_vertices) {
        port_vertice_poses.push_back(GetPos(sqn.order_, port_vertex));
    }
    GetKeyByPos(sdn.GetEmbedding(embedding_id), port_vertice_poses, key);

    auto& index = d == 0 ? su_index_ : sv_index_;

    if (index.key2group_id_.find(key) == index.key2group_id_.end()) {
        int group_id = AllocateGroupId(d);
        //std::cout << "Allocate new group id " << group_id << " for key " << key << std::endl;
        index.key2group_id_[key] = group_id;
    }

    int group_id = index.key2group_id_[key];
    // for (auto [key, id]: index.key2group_id_) {
    //     std::cout << "Key: " << key << ", Group ID: " << id << std::endl;
    // }
    index.group_id2embedding_ids_[group_id].push_back(embedding_id);
    while (embedding_id >= index.embedding_id2group_id_.size()) {
        index.embedding_id2group_id_.push_back(-1);
    }
    index.embedding_id2group_id_[embedding_id] = group_id;
}

void SuperDataEdge::Index::BuildKey2GroupId(SuperDataNode& sdn, SuperQueryNode& sqn, std::vector<ui>& port_vertices) {
    std::vector<int> port_vertice_poses;
    port_vertice_poses.reserve(sqn.GetVarNum());
    for (auto port_vertex : port_vertices) {
        port_vertice_poses.push_back(GetPos(sqn.order_, port_vertex));
    }
    for (int group_id = 0; group_id < group_id2embedding_ids_.size(); ++group_id) {
        auto embedding_id = group_id2embedding_ids_[group_id][0];
        std::vector<int> key;
        GetKeyByPos(sdn.GetEmbedding(embedding_id), port_vertice_poses, key);
        key2group_id_[key] = group_id;
    }
}


unsigned long long SuperDataEdge::MemoryCost() {
    auto index_cost = su_index_.MemoryCost() + sv_index_.MemoryCost();

    unsigned long long linked_groups_cost = 0;
    for (ui d = 0; d < 2; ++d) {
        for (auto& group : linked_groups_[d]) {
            linked_groups_cost += group.size() * sizeof(int);
        }
        linked_groups_cost += materialized_[d].size();  // vector<bool> is 1 bit per element
    }

    unsigned long long others = 2 * sizeof(int) /* su_, sv_*/ + 
                                2 * sizeof(bool) /* all_materialized_ */; 

    return index_cost + linked_groups_cost + others;
}

bool SuperDataEdge::IsMaterialized(int d, int eid) {
    auto& index = d == 0 ? su_index_ : sv_index_;
    auto& gid = index.embedding_id2group_id_[eid];
    return materialized_[d][gid];
}