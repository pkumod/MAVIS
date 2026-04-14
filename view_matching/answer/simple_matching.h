#ifndef SIMPLE_MATCHING_H
#define SIMPLE_MATCHING_H

#include <set>

#include "graph/graph.h"
#include "subgraph_filter.h"

class SimpleMatcher {
   public:
    Graph *g_;
    Graph *q_;
    std::vector<int> order_;
    std::vector<std::vector<int>> back_nbr_poss_;
    std::vector<bool> vis_;
    int state_limit_;
    int state_cnt_{};
    int embedding_limit_;

    CandidateSpace cs_;

    std::vector<std::vector<ui>> cand_;

    SimpleMatcher(Graph *data_graph, Graph *query_graph, int embedding_limit = 1, int state_limit = 10000) {
        g_ = data_graph;
        q_ = query_graph;
        embedding_limit_ = embedding_limit;
        state_limit_ = state_limit;
    }

    void Filter();

    void GetOrder();

    void GetLocalCandidate(int cur, std::vector<int> &embedding, std::vector<ui> &local_candidate);

    void Dfs(int cur, std::vector<int> &embedding, std::vector<std::vector<int>> &ret_embeddings);

    void Match(std::vector<std::vector<int>> &ret_embeddings, bool cand_set_provided = false);
};

#endif