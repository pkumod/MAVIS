#ifndef DFS_QUERY_DECOMPOSER_H
#define DFS_QUERY_DECOMPOSER_H

#include "abstract_query_decomposer.h"
#include "graph.h"
#include "subgraph_estimator.h"

using DPART = std::vector<ui>;
using DSTATE = std::vector<DPART>;

class TreePartitioner : public AbstractPartitioner {
   private:
    bool is_tree_ = true;
    Graph* query_graph_;
    SubgraphEstimator* estimator_;
    std::vector<int> UnionFind_;
    ui* all_nodes_ = nullptr;

    std::vector<int> dfs_no_;
    std::vector<std::vector<std::vector<ui>>> floyd_;
    int state_cnt_ = 0;

    bool need_connected_ = true;

    void TreePartition(std::vector<ui>& dfs_seq, std::vector<ui>& ret_state, long double& ret_score);
    void PartitionOnBCC(std::vector<ui>& bcc, std::vector<ui>& decomp);
    void GetDfsOrder(std::vector<ui>& dfs_seq, std::vector<ui>& bcc);

    void GetBccs(int u, int fr, int& cnt, std::vector<int>& dfn, std::vector<int>& low, std::vector<int>& stk, std::vector<std::vector<ui>>& bccs);

    long double EstimateStateEmbeddingNum(std::vector<ui>& state);
    bool CheckFutureConnectivity1(std::vector<ui>& state, ui cur, const std::vector<ui>& dfs_seq);
    void TreePartitionSearch(std::vector<ui>& cur_state, std::vector<ui>& edge_union, ui cur, const std::vector<ui>& dfs_seq,
                             ui lim_width, std::vector<std::vector<ui>>& next_states, std::vector<ui>& answer, long double& answer_score);

    int Find(int x) {
        if (UnionFind_[x] == x) return x;
        return UnionFind_[x] = Find(UnionFind_[x]);
    }

    int Union(int x, int y) {
        x = Find(x);
        y = Find(y);
        if (x == y) return 0;
        UnionFind_[x] = y;
        return 1;
    }

   public:
    /**
     * @param subgraph_estimator The subgraph estimator used to estimate the number of embeddings. if it is nullptr, the decomposer will not estimate the number of embeddings.
     * and the decomposition terminate when the first super tree is found.
     * @param query_graph The query graph to be decomposed.
     * @param need_connected If true, the decomposer will ensure that all super nodes are connected.
     */
    TreePartitioner(SubgraphEstimator* subgraph_estimator, Graph* query_graph, bool need_connected = true);
    ~TreePartitioner() override { delete[] all_nodes_; }
    void QueryDecomposition(std::vector<std::vector<ui>>& res) override;
};

#endif