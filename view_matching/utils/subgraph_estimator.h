#ifndef SUBGRAPH_ESTIMATOR_H
#define SUBGRAPH_ESTIMATOR_H

#include "MyUtils.h"
#include "graph.h"
#include "subgraph_filter.h"

using DPART = std::vector<ui>;

class SubgraphEstimator {
   private:
    Graph* query_graph_;
    Graph* data_graph_;
    std::map<DPART, long double> cache_;
    // std::vector<std::vector<ui>> candidate_sets_;
    CandidateSpace cs_;

    void EstimatePartEmbeddingNum(DPART& part, ui u, std::set<ui>& vis, std::map<PUU, long double>& path_count);

   public:
    SubgraphEstimator(Graph* query_graph, Graph* data_graph, CandidateSpace& cs)
        : query_graph_(query_graph), data_graph_(data_graph), cs_(cs) {}

    long double Estimate(ui node_bitset);
    long double Estimate(std::vector<ui>& nodes);
};

#endif