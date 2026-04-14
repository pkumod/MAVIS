#pragma once
#include <vector>
#include <map>
#include "super_data_node.h"
#include "super_query_graph.h"

class SuperDataEdge {
   public:
    class Index {
       public:
        std::vector<unsigned> embedding_id2group_id_;
        std::vector<std::vector<unsigned>> group_id2embedding_ids_;
        std::map<std::vector<int>, unsigned> key2group_id_; // this field is used for update

        unsigned long long MemoryCost();
        void BuildKey2GroupId(SuperDataNode& sdn, SuperQueryNode& sqn, std::vector<ui>& port_vertices);
    };

    int su_, sv_;
    Index su_index_, sv_index_;
    // linked_group_[0]: su -> sv, linked_group_[1]: sv -> su,
    // linked_group_[0][i] = {group_ids}: group i linked to group_ids
    std::vector<std::vector<int>> linked_groups_[2];

    std::vector<bool> materialized_[2];
    bool all_materialized_[2];  // all group edges are materialized

    void Dump(std::ofstream& fout);
    void Load(std::ifstream& fin, int su, int sv);

    int AllocateGroupId(int d);
    void InsertEmbedding(SuperDataNode& sdn, SuperQueryNode& sqn, std::vector<ui>& port_vertices, unsigned embedding_id, int d);
    bool CheckEmbedding(unsigned embedding_id, int d);

    bool IsMaterialized(int d, int eid);

    unsigned long long MemoryCost();
};

class SuperDataEdgeView {
    public:
    SuperDataEdge::Index& su_index_;
    SuperDataEdge::Index& sv_index_;
    std::vector<std::vector<int>>& linked_groups_;
    std::vector<std::vector<int>>& rev_linked_groups_;
    std::vector<bool>& materialized_;
    bool& all_materialized_;
    SuperDataEdgeView(SuperDataEdge& sde, int d) : su_index_(d == 0 ? sde.su_index_ : sde.sv_index_),
                                                   sv_index_(d == 0 ? sde.sv_index_ : sde.su_index_),
                                                   linked_groups_(sde.linked_groups_[d]),
                                                   rev_linked_groups_(sde.linked_groups_[1 - d]),
                                                   materialized_(sde.materialized_[d]),
                                                   all_materialized_(sde.all_materialized_[d]) {}
};