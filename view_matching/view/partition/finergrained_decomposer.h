#ifndef FINEGRANULAR_DECOMPOSER_1_H
#define FINEGRANULAR_DECOMPOSER_1_H
#include <map>
#include <vector>
#include "subgraph_estimator.h"
#include "graph.h"

using DPART = std::vector<unsigned>;
using DSTATE = std::vector<DPART>;

class FinergrainedDecomposer1 {
  SubgraphEstimator *estimator_;
  Graph *query_graph_;

  ui GetEdgeUnion(int u);
  ui GetNbrSuperNodeSet(ui u, const std::vector<ui>& other_parts);

 public:
  FinergrainedDecomposer1(Graph* query_graph, SubgraphEstimator* estimator) : query_graph_(query_graph), estimator_(estimator) {}
  void Exec(ui& part, std::vector<ui> other_parts, unsigned SCORE_LIMIT, std::vector<ui>& ret_parts);
};

#endif