#include "finergrained_decomposer.h"

#include "BitSet.h"
#include "pretty_print.h"

ui FinergrainedDecomposer1::GetEdgeUnion(int u) {
    ui edge_union = 0;
    ui len;
    const ui* nbrs = query_graph_->getVertexNeighbors(u, len);
    for (ui i = 0; i < len; ++i) BIT_SET(edge_union, nbrs[i]);
    return edge_union;
}

ui FinergrainedDecomposer1::GetNbrSuperNodeSet(ui part, const std::vector<ui>& other_parts) {
    ui ret = 0;
    for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
        if (BIT_TEST(part, i)) {
            ui edge_union = GetEdgeUnion(i);
            for (int j = 0; j < other_parts.size(); ++j) {
                if (edge_union & other_parts[j]) {
                    ret |= 1 << j;
                }
            }
        }
    }
    return ret;
}

void FinergrainedDecomposer1::Exec(ui& part, std::vector<ui> other_parts, unsigned SCORE_LIMIT, std::vector<ui>& ret_parts) {
    ret_parts.clear();
    for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
        if (BIT_TEST(part, i)) ret_parts.push_back(1 << i);
    }

    for (;;) {
        std::pair<ui, ui> best_edge = {0, 0};
        int max_dec = -1;
        for (int i = 0; i < ret_parts.size(); ++i) {
            auto& part1 = ret_parts[i];
            for (int j = i + 1; j < ret_parts.size(); ++j) {
                auto& part2 = ret_parts[j];

                auto part_edge_union = 0;
                for (int u = 0; u < query_graph_->getVerticesCount(); ++u) {
                    if (BIT_TEST(part1, u)) part_edge_union |= GetEdgeUnion(u);
                }

                if ((part_edge_union & part2) && estimator_->Estimate(part1 | part2) <= SCORE_LIMIT) {
                    ui nbr_super_node_set1 = GetNbrSuperNodeSet(part1, other_parts);
                    ui nbr_super_node_set2 = GetNbrSuperNodeSet(part2, other_parts);
                    auto dec = __builtin_popcount(nbr_super_node_set1) + __builtin_popcount(nbr_super_node_set2) - __builtin_popcount(nbr_super_node_set1 | nbr_super_node_set2);
                    if (max_dec < dec) {
                        max_dec = dec;
                        best_edge = {i, j};
                    }
                }
            }
        }

        if (max_dec < 0) break;

        auto& part1 = ret_parts[best_edge.first];
        auto& part2 = ret_parts[best_edge.second];
        part1 |= part2;
        std::swap(part2, ret_parts.back());
        ret_parts.pop_back();
    }
}
