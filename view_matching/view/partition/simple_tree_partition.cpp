#include "simple_tree_partition.h"

#include <algorithm>
#include <map>
#include <queue>

#include "finergrained_decomposer.h"
#include "global_vars.h"
#include "pretty_print.h"
#include "BitSet.h"

void SimpleTreePartition::Bfs() {
    std::queue<unsigned> q;
    level_ids_.resize(query_graph_->getVerticesCount(), -1);
    q.push(0);
    level_ids_[0] = 0;
    levels_.push_back({0});
    while (!q.empty()) {
        unsigned u = q.front();
        q.pop();
        unsigned len;
        auto nbrs = query_graph_->getVertexNeighbors(u, len);
        for (int i = 0; i < len; ++i) {
            auto v = nbrs[i];
            if (level_ids_[v] == -1) {
                level_ids_[v] = level_ids_[u] + 1;
                if (level_ids_[v] >= levels_.size()) {
                    levels_.push_back({});
                }
                levels_[level_ids_[v]].push_back(v);
                q.push(v);
            }
        }
    }
}


void SimpleTreePartition::QueryDecomposition(std::vector<std::vector<unsigned>>& res) {
    Bfs();
    std::vector<unsigned> super_node_ids;
    super_node_ids.resize(query_graph_->getVerticesCount());
    int level_num = levels_.size();
    union_find_.resize(query_graph_->getVerticesCount());
    for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
        union_find_[i] = i;
    }
    for (int i = level_num - 1; i >= 0; --i) {
        for (auto u : levels_[i]) {
            unsigned len;
            auto nbrs = query_graph_->getVertexNeighbors(u, len);
            for (int j = 0; j < len; ++j) {
                auto v = nbrs[j];
                if (level_ids_[v] == i) {
                    Union(u, v);
                } else if (level_ids_[v] == i + 1) {
                    auto sv = super_node_ids[v];
                    Union(u, res[sv][0]);
                }
            }
        }
        std::map<unsigned, std::vector<unsigned>> groups;
        for (auto u : levels_[i]) {
            groups[Find(u)].push_back(u);
        }
        for (auto group : groups) {
            res.push_back(group.second);
            for (auto u : group.second) {
                super_node_ids[u] = res.size() - 1;
            }
        }
        // clean union_find_
        for (auto u : levels_[i]) {
            union_find_[u] = u;
        }
        if (i + 1 < level_num) {
            for (auto u : levels_[i]) {
                union_find_[u] = u;
            }
        }
    }

    // std::cout << "original partition: " << res << std::endl;
    if (estimator_ != nullptr) {
        auto& decomp = res;
        std::map<unsigned, std::vector<unsigned>> adj;
        for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
            unsigned len;
            const unsigned* nbrs = query_graph_->getVertexNeighbors(i, len);
            for (int j = 0; j < len; ++j) {
                adj[i].push_back(nbrs[j]);
            }
        }
        // split unconnected components
        for (;;) {
            bool changed = false;
            for (int i = 0; i < res.size(); ++i) {
                auto& part = res[i];
                UnionFind<unsigned> uf(part);
                for (auto u : part) {
                    for (auto v : adj[u]) {
                        if (u > v) continue;
                        if (std::find(part.begin(), part.end(), v) == part.end()) continue;
                        uf.Union(u, v);
                        // std::cout << "union: " << u << " " << v << std::endl;
                    }
                }
                std::map<unsigned, std::vector<unsigned>> groups;
                for (auto u : part) groups[uf.Find(u)].push_back(u);
                // std::cout << "groups: " << groups << std::endl;
                if (groups.size() > 1) {
                    changed = true;
                    std::vector<std::vector<unsigned>> new_part;
                    for (auto group : groups) new_part.push_back(group.second);
                    res.erase(res.begin() + i);
                    res.insert(res.end(), new_part.begin(), new_part.end());
                    break;
                }
            }
            if (changed == false) break;
        }
        std::cout << "after split unconnected components: " << res << std::endl;

        // further partition
        std::cout << "hello?" << std::endl;
        std::vector<ui> decomp1;
        for (auto& part: decomp) {
            ui part_bitset = 0;
            for (auto u : part) {
                BIT_SET(part_bitset, u);
            }
            decomp1.push_back(part_bitset);
        }
        for (;;) {
            int exceed_limit_part_id = -1;
            long double est_val;
            for (ui i = 0; i < decomp1.size(); ++i) {
                est_val = estimator_->Estimate(decomp1[i]);
                if (est_val > MAX_EMBEDDING_NUM) {
                    exceed_limit_part_id = i;
                    break;
                }
            }
            if (exceed_limit_part_id == -1) break;

            auto exceed_limit_part = decomp1[exceed_limit_part_id];

            std::cout << "old part:";
            for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
                if (exceed_limit_part & (1 << i)) std::cout << " " << i;
            }
            std::cout << std::endl;
            std::cout << "estimate value: " << est_val << std::endl;

            decomp1.erase(decomp1.begin() + exceed_limit_part_id);

            std::unique_ptr<FinergrainedDecomposer1> fdc(new FinergrainedDecomposer1(query_graph_, estimator_));
            std::vector<ui> new_parts;
            fdc->Exec(exceed_limit_part, decomp1, MAX_EMBEDDING_NUM, new_parts);

            std::cout << "new parts: ";
            for (auto &part : new_parts) {
                for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
                    if (part & (1 << i)) std::cout << " " << i;
                }
                std::cout << " | ";
            }
            std::cout << std::endl;

            decomp1.insert(decomp1.end(), new_parts.begin(), new_parts.end());
        }

        decomp.clear();
        for (auto& part : decomp1) {
            std::vector<unsigned> new_part;
            for (int i = 0; i < query_graph_->getVerticesCount(); ++i) {
                if (part & (1 << i)) new_part.push_back(i);
            }
            decomp.push_back(new_part);
        }
        std::cout << "partition result: " << res << std::endl;
    }

}