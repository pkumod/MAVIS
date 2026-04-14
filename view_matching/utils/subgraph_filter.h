#ifndef SUBGRAPH_FILTER_H
#define SUBGRAPH_FILTER_H

#include "MyUtils.h"
#include "graph.h"
#include "relation/catalog.h"

class CandidateSpace {
   public:
    std::vector<std::vector<ui>> vcand_;
    std::vector<std::vector<std::vector<std::vector<ui>>>> ecand_;

    void GetCandidateEdges(unsigned u, unsigned v, std::vector<Edge>& ret);
    void Dump(std::string file_name);
    void Dump(std::ostream& os);

    void Load(std::string file_name);
    void Load(std::istream& is);
    unsigned long long MemoryCost();

    bool CheckEdgeExistence(ui qu, ui qv, ui du, ui dv);
};

class SubgraphFilter {
   private:
    Graph* query_graph_;
    Graph* data_graph_;
    bool isomorphic_;

    std::vector<ui> flag_;
    ui flag_cnt_;

    void Prune(unsigned u, unsigned v,
               std::vector<std::vector<unsigned>>& candidate_sets);

    bool Eliminate(bool& changed, std::vector<std::vector<unsigned>>& res);

    bool Build(std::vector<std::vector<unsigned>>& res);

   public:
    SubgraphFilter(Graph* query_graph, Graph* data_graph, bool isomorphic = true)
        : query_graph_(query_graph), data_graph_(data_graph), isomorphic_(isomorphic) {
        flag_.resize(data_graph_->getVerticesCount(), 0);
        flag_cnt_ = 0;
    }

    /**
     * build from scratch
     */
    bool Build(CandidateSpace& cs);

    void LinkEdge(CandidateSpace& cs);

    CandidateSpace BuildFromCandidates(std::vector<std::vector<unsigned>>& candidates);

    bool FilterFromEdge(std::vector<std::vector<unsigned>>& res, unsigned qu, unsigned qv, unsigned du, unsigned dv);
    bool FilterFromEdgeBatch(std::vector<std::vector<unsigned>>& res, unsigned qu, unsigned qv, std::vector<std::pair<unsigned, unsigned>>& edge_list);

    /**
     * merge src into dst
     */
    static void Merge(CandidateSpace& src, CandidateSpace& dst, Graph* query_graph);

    static void Remove(CandidateSpace& src, Graph* query_graph, ui qu, ui qv, ui du, ui dv);
};

#endif