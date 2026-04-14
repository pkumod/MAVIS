#ifndef SUPER_QUERY_GRAPH_H
#define SUPER_QUERY_GRAPH_H

#include <cassert>
#include <map>
#include <vector>

#include "MyUtils.h"
#include "graph.h"
#include "pretty_print.h"

class SuperQueryNode {
    private:
    bool HaveOuterLink(int u, Graph *query_graph);

   public:
    unsigned id_;
    std::vector<unsigned> node_list_;   // the inner nodes(aka. variables) of the super query node. It is sorted.
    std::vector<unsigned> order_;       // the order of vertices in the super query node. materilizated embeddings follow this order
    std::map<unsigned, unsigned> pos_;  // the position of the node in order_
    std::vector<std::vector<unsigned>> parents_;
    std::vector<unsigned> root_ids_;  // root_ids_[i] is the root of the i-th connected component
    std::map<unsigned, std::vector<unsigned>> link_info_;  // the link info of the inner nodes

    SuperQueryNode(int id, const std::vector<unsigned>& node_list, Graph *query_graph);


    // get the position of the var_id in node_list_, if not exist, return -1
    int GetVarPos(unsigned var_id) const { return GetPosInOrderList(node_list_, var_id); }

    // get number of variables
    int GetVarNum() const { return node_list_.size(); }

    unsigned GetRootId() const { return root_ids_[0]; }

    unsigned GetRootPos() const { return GetVarPos(GetRootId()); }

    unsigned GetEmbeddingWidth() const { return node_list_.size(); }

    unsigned CheckVarInSQN(unsigned var_id) const {
        return GetPosInOrderList(node_list_, var_id) != -1;
    }
};

class SuperQueryGraph {
   public:
    std::unique_ptr<Graph> query_graph_;
    std::vector<SuperQueryNode> sqns_;  // the super query nodes

    struct SuperEdgeInfo {
        unsigned dst;
        std::vector<unsigned> src_set;  // the link-out nodes of src
        std::vector<unsigned> dst_set;  // the link-in nodes of dst
        std::vector<PUU> src_dst_pairs;
    };
    std::vector<std::vector<SuperEdgeInfo>> adj_;  // the adjacency list of the super query node
    std::vector<std::vector<SuperEdgeInfo>> adj_mat_;  // the adjacency matrix of the super query node

    SuperQueryGraph() = default;
    SuperQueryGraph(SuperQueryGraph&& other) = default;
    SuperQueryGraph(const SuperQueryGraph& other) = delete;
    SuperQueryGraph& operator=(const SuperQueryGraph& other) = delete;
    SuperQueryGraph& operator=(SuperQueryGraph&& other) = default; 

    virtual ~SuperQueryGraph() = default;
};

class TreePartitionGraph: public SuperQueryGraph {
private:

   public:
    TreePartitionGraph(std::vector<std::vector<ui>>& super_nodes, Graph* query_graph) {
        query_graph_ = std::unique_ptr<Graph>(query_graph);
        Build(super_nodes);
    }
    TreePartitionGraph() {}

    int GetId2SQNId(unsigned id) const {
        for (unsigned i = 0; i < sqns_.size(); ++i) {
            if (sqns_[i].GetVarPos(id) != -1) return i;
        }
        return -1;
    }

    /**
     * Build the super query graph from the tree partition
     * @param super_nodes: the super nodes, each super node is a vector of variable
     */
    void Build(const std::vector<std::vector<ui>>& super_nodes);

    void PrintSQNs();
    void PrintSQEdges();
    void Print();

    void PrintDot(std::ofstream& fout);
    void PrintDot(std::string file_name);

    void Init();

    void Dump(std::string file_name);
    void Dump(std::ofstream& fout);
    void Load(std::string file_name);
    void Load(std::ifstream& fin);

    void Copy(const TreePartitionGraph& other);
};

class TreeDecompositionGraph : public TreePartitionGraph {
   public:

    void Build(const std::vector<std::vector<ui>>& super_nodes, const std::vector<std::vector<ui>>& links);

    void PrintDot(std::string file_name);

    void Dump(std::ofstream & fout) {
        int edge_num = 0;
        for (int i = 0; i < sqns_.size(); ++i) {
            edge_num += adj_[i].size();
        }
        fout << sqns_.size() << " " << edge_num / 2 << std::endl;
        for (int i = 0; i < sqns_.size(); ++i) {
            fout << sqns_[i].id_ << " " << sqns_[i].GetEmbeddingWidth() << std::endl;
            for (auto& node : sqns_[i].node_list_) {
                fout << node << " ";
            }
            fout << std::endl;
        }
        for (int i = 0; i < sqns_.size(); ++i) {
            for (auto& edge_info : adj_[i]) {
                if (i >= edge_info.dst) continue;  // avoid duplicate edges
                fout << i << " " << edge_info.dst << "\n";
            }
        }
    }

    void Load(std::ifstream & fin, Graph *view_pattern) {
        int sqn_num, edge_num;
        fin >> sqn_num >> edge_num;
        std::vector<std::vector<unsigned>> super_nodes(sqn_num);
        std::vector<std::vector<unsigned>> links(sqn_num);
        for (int i = 0; i < sqn_num; ++i) {
            int id, embedding_width;
            fin >> id >> embedding_width;
            for (int j = 0; j < embedding_width; ++j) {
                auto node_id = 0;
                fin >> node_id;
                super_nodes[i].push_back(node_id);
            }
        }
        for (int i = 0; i < edge_num; ++i) {
            int u, v;
            fin >> u >> v;
            links[u].push_back(v);
            links[v].push_back(u);  // undirected graph
        }
        std::cout << "super_nodes: " << super_nodes << "\nedges: " << links << std::endl;
        this->query_graph_ = std::unique_ptr<Graph>(view_pattern->Clone());
        Build(super_nodes, links);
    }
};

#endif
