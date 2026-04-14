#pragma once
#include "auxilary_info.h"
#include "rewrite_query.h"

#define USE_NEC

class Preprocessor {
   public:
    RewriteQuery& rq_;
    AuxilaryInfo& aux_;
    Graph* query_graph_;
    Graph* data_graph_;

    std::vector<ui> input_rw_order_;

    Preprocessor(RewriteQuery& rq, AuxilaryInfo& aux, Graph* query_graph, Graph* data_graph)
        : rq_(rq), aux_(aux), query_graph_(query_graph), data_graph_(data_graph) {}

    void GetNoRepeatLabel(Graph* query_graph);
    void CalcParentRelatedInfo();
    void HandleNeighborEquivalenceClass(Graph* query_graph, UnionFind<ui>& rw_uf);
    void CalcCandidateSets(catalog* storage, const Graph* query_graph, ui data_vertex_count);
    void FilterOnQ(Graph* query_graph, Graph* data_graph, catalog* storage);
    void Exec();

    void PrintInfo();
};