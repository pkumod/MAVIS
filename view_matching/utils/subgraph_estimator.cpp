#include "subgraph_estimator.h"

#include <map>
#include <set>

void SubgraphEstimator::EstimatePartEmbeddingNum(DPART &part, unsigned u, std::set<unsigned> &vis, std::map<PUU, long double> &path_count) {
    vis.insert(u);
    std::vector<unsigned> children;
    unsigned len;
    const unsigned *ns = query_graph_->getVertexNeighbors(u, len);
    for (int i = 0; i < len; ++i) {
        auto v = ns[i];
        if (vis.find(v) != vis.end()) continue;
        if (std::find(part.begin(), part.end(), v) == part.end()) continue;
        EstimatePartEmbeddingNum(part, v, vis, path_count);
        children.push_back(v);
    }

    const auto &u_cand = cs_.vcand_[u];
    for (auto x : u_cand) {
        path_count[std::make_pair(u, x)] = 1;
    }

    for (auto v : children) {
        const auto &u_cand = cs_.vcand_[u];
        const auto &v_cand = cs_.vcand_[v];
        for (auto x : u_cand) {
            unsigned y_num;
            const unsigned *ys = data_graph_->getVertexNeighbors(x, y_num);
            std::vector<unsigned> y_vec(ys, ys + y_num);
            IntersectSortedLists(y_vec, v_cand);
            long double sum = 0;
            for (auto y : y_vec) {
                sum += path_count[std::make_pair(v, y)];
            }
            path_count[std::make_pair(u, x)] *= sum;
        }
    }
}

long double SubgraphEstimator::Estimate(std::vector<unsigned> &nodes) {
    std::vector<unsigned> nodes1 = nodes;
    std::sort(nodes1.begin(), nodes1.end());
    if (cache_.count(nodes1)) return cache_[nodes1];

    assert(cs_.vcand_.size());

    std::map<PUU, long double> path_count;
    std::set<unsigned> vis;

    long double ret = 1;
    for (auto node : nodes) {
        if (vis.find(node) != vis.end()) continue;
        EstimatePartEmbeddingNum(nodes, node, vis, path_count);
        long double sum = 0;
        auto &candidates = cs_.vcand_[node];
        for (auto x : candidates) {
            sum += path_count[std::make_pair(node, x)];
        }
        ret *= sum;
    }

    cache_[nodes1] = ret;

    return ret;
}

long double SubgraphEstimator::Estimate(ui node_bitset) {
    std::vector<unsigned> nodes;
    for (unsigned i = 0; i < query_graph_->getVerticesCount(); ++i) {
        if (node_bitset & (1 << i)) {
            nodes.push_back(i);
        }
    }
    return Estimate(nodes);
}