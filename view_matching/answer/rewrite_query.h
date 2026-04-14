#ifndef REWRITE_QUERY_H
#define REWRITE_QUERY_H

#include <fstream>
#include <map>

#include "graph/graph.h"
#include "multilayer_trie.h"
#include "pretty_print.h"
#include "relation/catalog.h"
#include "BitSet.h"
#include "view/storage/mat_view.h"

class RewriteQuery {
   public:
    enum Type { NORMAL, MATERIALIZED };

    struct Node {
        Type type_;

        // normal node
        int node_id_;

        // MatNode
        SuperDataNode* sdn_;
        SuperQueryNode* sqn_;

        std::map<int, int> qv2sv_, sv2qv_;             /* mapping_: mapping the node id in super node to node id in query graph, rev_mapping_ is reverse*/
        std::vector<std::pair<int, int>> patch_edges_; /* the edges that need to be patched, denoted by super node id */
        std::vector<std::pair<int, int>> patch_edges1_; /* the edges that need to be patched, denoted by super node id */

        std::unique_ptr<int[]> memory_pool_; /* memory pool for the encoded embeddings */
        std::vector<int*> encoded_embeddings_;
        std::vector<int> patch_edge_passed_; /* for each embedding, record which patch edges are passed 0 -- unknown, 1 -- passed, -1 -- failed */

        std::vector<ui> node_list_; // node list which is arranged in the order same as corresponding super query node

        Node(int node_id = -1) {
            if (node_id == -1) {
                type_ = Type::MATERIALIZED;
                node_id_ = -1;
            } else {
                type_ = Type::NORMAL;
                node_id_ = node_id;
                node_list_.push_back(node_id);
            }
        }

        Node(Node&& x) = default;
        Node& operator=(Node&& x) = default;

        void PrintInfo() {
            if (type_ == Type::MATERIALIZED) {
                std::cout << "MatNode, node_id: " << node_id_ << ", sqn id: " << sqn_->id_ << std::endl;
                for (auto& kv : qv2sv_) {
                    std::cout << kv.second << " <- " << kv.first << ", ";
                }
                std::cout << std::endl;
            } else {
                std::cout << "NormalNode, node_id: " << node_id_ << std::endl;
            }
        }

        const std::vector<ui>& GetInnerOrder() { return node_list_; }
        int GetNodeNum() { return node_list_.size(); }
        int GetSingleNode() { return node_list_.front(); }
        const std::vector<ui>& GetNodeList() { return node_list_; }
    };

    struct REdge {
        int src_;
        int dst_;
        Type type_;
        REdge(int src, int dst, Type type) : src_(src), dst_(dst), type_(type) {}
        virtual ~REdge() {}
    };

    struct MatEdge : public REdge {
        TreePartitionGraph::SuperEdgeInfo* super_edge_info_;
        SuperDataEdge* super_edge_cand_;
        bool d_;
        std::vector<std::pair<ui,ui>> patch_edges_; 
        int covered_edge_num_{0};
        std::vector<ui> src_port_vertices_, dst_port_vertices_;

        MatEdge(int src, int dst, TreePartitionGraph::SuperEdgeInfo* super_edge_info, SuperDataEdge* new_super_edge, 
            bool d, std::vector<std::pair<ui,ui>> patch_edges = {}, int covered_edge_num = 0, std::vector<ui> src_port_vertices = {}, std::vector<ui> dst_port_vertices = {})
            : REdge(src, dst, Type::MATERIALIZED),
              super_edge_info_(super_edge_info),
              super_edge_cand_(new_super_edge),
              d_(d),
              patch_edges_(std::move(patch_edges)),
              covered_edge_num_(covered_edge_num),
              src_port_vertices_(std::move(src_port_vertices)),
              dst_port_vertices_(std::move(dst_port_vertices)) {
            type_ = Type::MATERIALIZED;
        }
    };

    struct NormalEdge : public REdge {
        NormalEdge(int src, int dst): REdge(src, dst, Type::NORMAL) { }
    };

    std::vector<Node*> nodes_;
    std::vector<unsigned> rw_node_no_;
    std::vector<std::vector<REdge*>> link_;
    std::unique_ptr<Graph> rw_graph_;

    // vertex-based view
    std::vector<std::vector<std::vector<Edge>>> filtered_edge_lists_;
    std::vector<bool> need_nlf_;

    int BelongToWhichRWNode(int node_id) {
        return rw_node_no_[node_id];
    }

    bool BelongToMatNode(int node_id) {
        int rw_node_id = BelongToWhichRWNode(node_id);
        return nodes_[rw_node_id]->type_ == Type::MATERIALIZED;
    }

    friend void DeepCopy(const RewriteQuery& src, RewriteQuery& dst) {
        dst.nodes_.clear();
        for (auto& node : src.nodes_) {
            dst.nodes_.push_back(new Node(std::move(*node)));
        }
        dst.link_.resize(src.link_.size());
        for (int i = 0; i < src.link_.size(); ++i) {
            for (auto& edge : src.link_[i]) {
                if (edge->type_ == Type::MATERIALIZED) {
                    dst.link_[i].push_back(new MatEdge(*dynamic_cast<MatEdge*>(edge)));
                } else {
                    dst.link_[i].push_back(new NormalEdge(*dynamic_cast<NormalEdge*>(edge)));
                }
            }
        }
    }

    RewriteQuery() = default;
    RewriteQuery(RewriteQuery&& x) {
        DeepCopy(x, *this);
        rw_graph_ = std::move(x.rw_graph_);
        rw_node_no_ = std::move(x.rw_node_no_);
        filtered_edge_lists_ = std::move(x.filtered_edge_lists_);
        need_nlf_ = std::move(x.need_nlf_);
    }
    RewriteQuery& operator=(RewriteQuery&& x) {
        DeepCopy(x, *this);
        rw_graph_ = std::move(x.rw_graph_);
        rw_node_no_ = std::move(x.rw_node_no_);
        filtered_edge_lists_ = std::move(x.filtered_edge_lists_);
        need_nlf_ = std::move(x.need_nlf_);
        return *this;
    }

    ~RewriteQuery() {
        for (auto node : nodes_) {
            delete node;
        }
        for (auto& edges : link_) {
            for (auto edge : edges) {
                delete edge;
            }
        }
    }

    void PrintDot(std::ostream& out, Graph* query_graph);
};

auto RewriteQueryFromMatViewSet(Graph* query_graph,
                                std::vector<MatView>& mv_set);

void RewriteQueryFromMatView(Graph* query_graph, MatView& mv, std::vector<int>& vnode2qnode,
                             RewriteQuery& rq);

struct ProcessQueryResult {
    long double embedding_num;
    long long enum_cost;
    long long encode_cost;
    long long preprocess_cost;
    long long rewrite_cost;
    bool time_out;
};

ProcessQueryResult ProcessQuery(Graph* data_graph, Graph* query_graph,
                                std::vector<MatView>& mv_set,
                                long long enum_limit, int enum_time_limit);

#endif