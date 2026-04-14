#pragma once

#include "graph/graph.h"
#include "multilayer_trie.h"
#include "rewrite_query.h"

enum EnumMethod {
    NormalNode = 0,
    MatEdge = 1,
    Mix = 2,
    Trie = 3
};

class AuxilaryInfo {
   public:
    Graph* query_graph_;
    Graph* data_graph_;
    std::vector<unsigned> rw_order_;  // the order of nodes in the rewrite query
    std::vector<std::vector<unsigned>> parents_;
    std::vector<std::vector<RewriteQuery::REdge*>> parent_edges_, parent_rev_edges_;  // the edge that connects to parent node
    std::vector<EnumMethod> enum_methods_;  // the enumeration method of each rewrite node
    std::vector<unsigned> query_node_order_;  // the order of nodes in the query graph
    std::vector<unsigned> query_node_no_;  // the node number of each node in the query graph

    std::vector<std::vector<unsigned>> parent_query_nodes_;  // the backward neighbors of each vertices, except the vertices in a same rewrite node.

    std::vector<std::vector<ui>> port_vertices_; // for each rewrite node, record its port vertices in query graph
    std::vector<std::vector<ui>> non_port_vertices_; // for each rewrite node, record its non-port vertices in query graph

    std::vector<std::vector<unsigned>> candidates_;
    std::vector<bool> no_repeat_label_;
    unsigned long long need_idx_ = 0;

    // neighbor equivalence class related
    std::vector<int> q_nec_count_;
    std::vector<int> rq_nec_count_;
    std::vector<bool> first_of_nec_;

    std::vector<NewTrie*> tries1_;

    bool enable_output = false;
    std::unique_ptr<catalog> storage_;

    long long num_limit_ = 0;

    AuxilaryInfo() = default;
    AuxilaryInfo(AuxilaryInfo&&) = default;
};