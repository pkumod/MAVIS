#pragma once

#include <vector>
#include "mat_view.h"
#include "BitSet.h"

class ViewFilter {
    SuperQueryGraph& sqg_;
    std::vector<SuperDataNode>& sdns_;
    std::vector<std::vector<SuperDataEdge>>& ses_;

    // std::unique_ptr<MyBitSet> bs;

    ui check_count_{};


    void CalcNewNodeIds(unsigned su, std::vector<int>& new_node_ids);
    void UpdateIndex(unsigned su, unsigned sv, const std::vector<int>& new_node_ids, std::vector<int>& new_group_ids);
    void UpdateLink(unsigned su, unsigned sv, std::vector<int>& su_new_group_ids, std::vector<int>& sv_new_group_ids);
    void RemoveInvalidEmbeddings(unsigned su, unsigned sv);
    int FilterBy(unsigned u, unsigned v);
    bool CheckEdgeValid(ui su, ui sv, ui su_ebd_id);

public:
    ViewFilter(SuperQueryGraph& sqg, std::vector<SuperDataNode>& sdns, std::vector<std::vector<SuperDataEdge>>& ses, int data_graph_size)
        : sqg_(sqg), sdns_(sdns), ses_(ses) {
            // bs = std::make_unique<MyBitSet>(data_graph_size);
        }
    bool FilterEmbeddings();
    void RemoveInvalidEmbeddings();
};