#ifndef SUBGRAPHMATCHING_NLF_FILTER_H
#define SUBGRAPHMATCHING_NLF_FILTER_H

#include <vector>

#include "graph/graph.h"
#include "utility/relation/catalog.h"

class nlf_filter {
   private:
    const Graph* query_graph_;
    const Graph* data_graph_;
    std::vector<char> status_;
    std::vector<uint32_t> updated_;

   private:
    void filter_ordered_relation(uint32_t u, edge_relation* relation);
    void filter_unordered_relation(uint32_t u, edge_relation* relation);

   public:
    nlf_filter(const Graph* query_graph, const Graph* data_graph) {
        query_graph_ = query_graph;
        data_graph_ = data_graph;
        status_.resize(data_graph->getVerticesCount(), 'u');
        updated_.reserve(1024);
    }

    static bool cmp(const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) {
        return a.first < b.first;
    }
    static bool check(const std::vector<std::pair<uint32_t, uint32_t>>* u_nlf, const std::vector<std::pair<uint32_t, uint32_t>>* v_nlf);

    void execute(std::vector<std::vector<uint32_t>>& candidate_sets);
    void execute(catalog* storage, std::vector<bool>& need_nlf);
    void execute(catalog* storage);
};

#endif  // SUBGRAPHMATCHING_NLF_FILTER_H
