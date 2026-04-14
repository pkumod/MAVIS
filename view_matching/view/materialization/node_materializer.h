#ifndef NODE_MATERIALIZER_H
#define NODE_MATERIALIZER_H

#include "mat_view.h"
#include "subgraph_estimator.h"
#include "subgraph_filter.h"

class NodeMaterializer {
    friend class Materializer;
    std::vector<SuperDataNode>& sdns_;
    SubgraphEstimator* estimator_;
    std::vector<SuperQueryNode>& sqns_;
    CandidateSpace cs_;

    bool & oversized_;


    void MultisetIntersectionBf(std::vector<const unsigned*>& lists, std::vector<unsigned>& list_lens, std::vector<unsigned>& ret);
    void GetNextNodeCandidates(SuperQueryNode& sqn, unsigned cur, std::unique_ptr<unsigned[]>& embedding, std::vector<unsigned>& next_candidates);
    void SearchSQNEmbeddings(SuperQueryNode& sqn, SuperDataNode& sdn, unsigned cur, std::unique_ptr<unsigned[]>& embedding);
    bool MaterializeSDN(unsigned sqn_id);

   public:
    NodeMaterializer(std::vector<SuperDataNode>& sdns, 
        std::vector<SuperQueryNode>& sqns, SubgraphEstimator* estimator, CandidateSpace& cs, bool& oversized)
        : sdns_(sdns), sqns_(sqns), estimator_(estimator), cs_(cs), oversized_(oversized) { }
    bool MaterializeSDNs();
};

#endif