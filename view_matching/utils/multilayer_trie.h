#ifndef MULTILAYER_TRIE_H
#define MULTILAYER_TRIE_H

#include <cassert>
#include <vector>
#include <unordered_map>

class MultiLayerTrie {
   public:
    int layer_num_;
    std::vector<std::vector<unsigned>> columns_;
    std::vector<std::vector<unsigned>> offset_;
    std::vector<unsigned> row_ids_;
    std::vector<std::vector<std::vector<unsigned>>> leaves_;

    MultiLayerTrie(int layer_num, std::vector<unsigned*> rows, std::vector<unsigned> row_ids);

    const unsigned* GetChidren(int layer_id, int idx, int& len) {
        assert(layer_id < layer_num_ - 1);
        assert(idx < offset_[layer_id].size());
        len = offset_[layer_id][idx + 1] - offset_[layer_id][idx];
        return &columns_[layer_id + 1][offset_[layer_id][idx]];
    }

    int GetIdxChildren(int layer_id, int idx, int& len) {
        assert(layer_id < layer_num_ - 1);
        assert(idx < offset_[layer_id].size());
        len = offset_[layer_id][idx + 1] - offset_[layer_id][idx];
        return offset_[layer_id][idx];
    }

    unsigned GetValue(int layer_id, int idx) {
        return columns_[layer_id][idx];
    }

    void Dfs(int cur, std::vector<int>& idxs, std::vector<unsigned*>& rows, std::vector<unsigned>& row_ids);
    void GetAllRows(std::vector<unsigned*>& rows, std::vector<unsigned>& row_ids);

    void Insert(unsigned* row, unsigned row_id);
    void Delete(unsigned* row);

    int GetRowId(unsigned* row);
};


class NewTrie {
public:
    class TrieNode {
        public:
        std::unordered_map<unsigned, unsigned> children_; // key: value, value: index in nodes_
        int row_id_ = -1;
    };

    std::vector<TrieNode> pool_;
    int column_num_;

    NewTrie(int column_num, bool incremental) : column_num_(column_num) {
        if (incremental == false) pool_.reserve(1000000);
        pool_.push_back(TrieNode()); // root
    }

    int GetRootId() {
        return 0;
    }

    int GetRowId(unsigned* row);

    void Insert(unsigned* row, unsigned row_id);

    void Delete(unsigned* row);

    unsigned long long GetMemoryCost() {
        unsigned long long cost = sizeof(NewTrie);
        cost += pool_.size() * sizeof(TrieNode);
        for (auto& node : pool_) {
            cost += node.children_.size() * (sizeof(unsigned) + sizeof(unsigned));
        }
        return cost;
    }
};

#endif