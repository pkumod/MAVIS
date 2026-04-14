#ifndef MAT_VIEW_BUILDER_H
#define MAT_VIEW_BUILDER_H

#include "types.h"
#include "mat_view.h"
#include "subgraph_estimator.h"
#include "subgraph_filter.h"

class MatViewBuilder {
   public:
    MatViewBuilder() {}

    unsigned long long view_evaluation_cost_;
    bool oversized_ = false;

    bool Build(Graph* view_pattern, Graph* data_graph, MatView& mv, bool naive_partition = false);
    std::string BuildWithinTimeLimit(Graph* view_pattern, Graph* data_graph, MatView& mv, unsigned time_limit);

    bool Materialize(Graph* view_pattern, Graph* data_graph, SubgraphEstimator* estimator,
         TreePartitionGraph& sqg, CandidateSpace& cs, MatView& mv, bool incremental = false);
};


#endif
