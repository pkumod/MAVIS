#ifndef SIMPLE_TREE_PARTITION_H
#define SIMPLE_TREE_PARTITION_H

#include <vector>

#include "abstract_query_decomposer.h"
#include "graph.h"
#include "subgraph_estimator.h"

class SimpleTreePartition : public AbstractPartitioner {
   public:
    Graph *query_graph_;
    SubgraphEstimator *estimator_;
    std::vector<std::vector<unsigned>> levels_;
    std::vector<int> level_ids_;
    std::vector<int> union_find_;

    int Find(int x) {
        if (union_find_[x] == x) return x;
        return union_find_[x] = Find(union_find_[x]);
    }

    int Union(int x, int y) {
        x = Find(x);
        y = Find(y);
        if (x == y) return 0;
        union_find_[x] = y;
        return 1;
    }

    SimpleTreePartition(SubgraphEstimator *estimator, Graph *query_graph) : estimator_(estimator), query_graph_(query_graph) {}

    void Bfs();

    void QueryDecomposition(std::vector<std::vector<ui>> &res) override;
};

#endif