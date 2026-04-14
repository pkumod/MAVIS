#pragma once

#include "auxilary_info.h"
#include "rewrite_query.h"

class Planner {
   public:
    RewriteQuery& rq_;
    AuxilaryInfo& aux_;
    Graph* query_graph_;
    Graph* data_graph_;

    double eps = 1e-6;

    Planner(RewriteQuery& rq, AuxilaryInfo& aux, Graph* query_graph, Graph* data_graph)
        : rq_(rq), aux_(aux), query_graph_(query_graph), data_graph_(data_graph) {}

    void BuildCoreGraph(Graph* g, Graph*& core_g, std::vector<unsigned>& core_vertices);

    int GetCandidateSize(ui node_id);
    int Cmp(double a);
    void GetOrder();
    void GetOrder2();
    void GetOrder3();
    void GetOrder4();
    void GetOrder4_ori();
    void GetOrder5();
    void GetOrder6();
    void GetCoreOrder();
    void GetOrderOPV();
};