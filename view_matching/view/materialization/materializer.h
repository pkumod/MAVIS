#pragma once

#include "types.h"
#include "mat_view.h"
#include "subgraph_estimator.h"
#include "subgraph_filter.h"

class Materializer {
   public:
    TreePartitionGraph sqg_;
    Graph *query_graph_, *data_graph_;
    SubgraphEstimator* estimator_;

    CandidateSpace cs_;

    std::vector<std::vector<bool>> complete_;

    std::vector<SuperDataNode> sdns_;
    std::vector<std::vector<SuperDataEdge>> ses_;

    bool oversized_ = false;

   private:


    void OutputGroupEmbeddings(int su, std::vector<ui>& ids);
    void OutputAllLinks();

    bool CheckCompleteness();

   public:
    unsigned long long filter_cost_ = 0;
    unsigned long long materialize_cost_ = 0;
    unsigned long long link_superedge_cost_ = 0;
    unsigned long long view_evaluation_cost_ = 0;
    unsigned long long get_embedding_count_ = 0;
    unsigned long long filter_embedding_cost_ = 0;
    unsigned long long find_l_count_ = 0;

   public:

    Materializer(Graph* query_graph, Graph* data_graph, SubgraphEstimator* estimator, TreePartitionGraph& sqg, CandidateSpace& cs);

    bool Exec();

};