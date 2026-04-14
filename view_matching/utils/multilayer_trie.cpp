#include "multilayer_trie.h"

#include <iostream>

#include "pretty_print.h"

MultiLayerTrie::MultiLayerTrie(int layer_num, std::vector<unsigned*> rows,
                               std::vector<unsigned> row_ids) {
    layer_num_ = layer_num;
    columns_.resize(layer_num);
    offset_.resize(layer_num - 1);
    leaves_.resize(layer_num);

    auto row_num = rows.size();

    for (int i = 0; i < rows.size(); ++i) {
        // auto id = order[i];
        auto id = i;
        auto row = rows[id];
        int p = -1;
        for (int j = 0; j < layer_num; ++j) {
            if (columns_[j].empty() || row[j] != columns_[j].back()) {
                assert(columns_[j].empty() || row[j] > columns_[j].back());
                p = j;
                break;
            }
        }
        assert(p != -1);
        for (int j = p; j < layer_num; ++j) {
            columns_[j].push_back(row[j]);
            if (j != layer_num - 1) {
                offset_[j].push_back(columns_[j + 1].size());
            }
        }
    }

    for (int i = 0; i < layer_num - 1; ++i) {
        offset_[i].push_back(columns_[i + 1].size());
    }

    // row_ids_ = row_ids;
    row_ids_.reserve(row_ids.size());
    for (int i = 0; i < row_ids.size(); ++i) {
        // row_ids_.push_back(row_ids[order[i]]);
        row_ids_.push_back(row_ids[i]);
        leaves_[layer_num - 1].push_back({row_ids[i]});
    }

    for (int layer = layer_num - 2; layer >= 0; --layer) {
        int num_nodes = offset_[layer].size() - 1;
        leaves_[layer].resize(num_nodes);
        for (int node_idx = 0; node_idx < num_nodes; ++node_idx) {
            int start = offset_[layer][node_idx];
            int end = offset_[layer][node_idx + 1];
            for (int child_idx = start; child_idx < end; ++child_idx) {
                auto& child_leaves = leaves_[layer + 1][child_idx];
                leaves_[layer][node_idx].insert(
                    leaves_[layer][node_idx].end(),
                    child_leaves.begin(), child_leaves.end());
            }
        }
    }
} 

void MultiLayerTrie::Insert(unsigned* row, unsigned row_id) {
    // use bruteforce way: rebuild
    std::vector<unsigned*> ret_rows;
    std::vector<unsigned> ret_row_ids;
    GetAllRows(ret_rows, ret_row_ids);
    int insert_pos = ret_rows.size();
    for (int i = 0; i < ret_rows.size(); ++i) {
        for (int j = 0; j < layer_num_; ++j) {
            if (row[j] != ret_rows[i][j]) {
                if (row[j] < ret_rows[i][j]) insert_pos = i;
                break;
            }
        }
        if (insert_pos == i) break;
    }
    ret_rows.insert(ret_rows.begin() + insert_pos, row);
    ret_row_ids.insert(ret_row_ids.begin() + insert_pos, row_id);
    MultiLayerTrie new_trie(layer_num_, ret_rows, ret_row_ids);
    *this = new_trie;
}

void MultiLayerTrie::Delete(unsigned* row) {
    // use bruteforce way: rebuild
    std::vector<unsigned*> ret_rows;
    std::vector<unsigned> ret_row_ids;
    GetAllRows(ret_rows, ret_row_ids);
    int delete_pos = -1;
    for (int i = 0; i < ret_rows.size(); ++i) {
        bool equal = true;
        for (int j = 0; j < layer_num_; ++j) {
            if (row[j] != ret_rows[i][j]) {
                equal = false;
                break;
            }
        }
        if (equal) {
            delete_pos = i;
            break;
        }
    }
    assert(delete_pos != -1);
    ret_rows.erase(ret_rows.begin() + delete_pos);
    ret_row_ids.erase(ret_row_ids.begin() + delete_pos);
    MultiLayerTrie new_trie(layer_num_, ret_rows, ret_row_ids);
    *this = new_trie;
}

void MultiLayerTrie::Dfs(int cur, std::vector<int>& idxs, std::vector<unsigned*>& rows, std::vector<unsigned>& row_ids) {
    // std::cout << cur << ": " << idxs << std::endl;
    if (cur == layer_num_) {
        unsigned* r = new unsigned[layer_num_];
        for (int i = 0; i < layer_num_; ++i) {
            r[i] = GetValue(i, idxs[i]);
        }
        rows.push_back(r);
        row_ids.push_back(row_ids_[idxs[layer_num_ - 1]]);
        return;
    }
    int len;
    int start = GetIdxChildren(cur - 1, idxs[cur - 1], len);
    for (int i = start; i < start + len; ++i) {
        idxs.push_back(i);
        Dfs(cur + 1, idxs, rows, row_ids);
        idxs.pop_back();
    }
}

void MultiLayerTrie::GetAllRows(std::vector<unsigned*>& rows, std::vector<unsigned>& row_ids) {
    for (int i = 0; i < columns_[0].size(); ++i) {
        std::vector<int> row;
        row.push_back(i);
        Dfs(1, row, rows, row_ids);
        row.pop_back();
    }
}

int MultiLayerTrie::GetRowId(unsigned* row) {
    int cur_layer = 0, last_idx = -1, ubnd = columns_[0].size(), lbnd = 0;
    while (cur_layer < layer_num_) {
        if (cur_layer == 0) {
            lbnd = 0;
            ubnd = columns_[0].size();
        }
        else {
            lbnd = offset_[cur_layer - 1][last_idx];
            ubnd = offset_[cur_layer - 1][last_idx + 1];
        }
        int pos = std::lower_bound(columns_[cur_layer].begin() + lbnd, columns_[cur_layer].begin() + ubnd, row[cur_layer]) - columns_[cur_layer].begin();
        if (pos == ubnd || columns_[cur_layer][pos] != row[cur_layer]) return -1;
        last_idx = pos;
        cur_layer++;
    }
    return row_ids_[last_idx];
}

int NewTrie::GetRowId(unsigned* row) {
    int cur_node_id = GetRootId();
    for (int i = 0; i < column_num_; ++i) {
        auto& cur_node = pool_[cur_node_id];
        auto it = cur_node.children_.find(row[i]);
        if (it == cur_node.children_.end()) {
            return -1;
        }
        cur_node_id = it->second;
    }
    return pool_[cur_node_id].row_id_;
}

void NewTrie::Insert(unsigned* row, unsigned row_id) {
    int cur_node_id = GetRootId();
    for (int i = 0; i < column_num_; ++i) {
        auto& cur_node = pool_[cur_node_id];
        auto it = cur_node.children_.find(row[i]);
        if (it == cur_node.children_.end()) {
            pool_.emplace_back();
            unsigned new_node_id = pool_.size() - 1;
            auto& cur_node = pool_[cur_node_id]; // since pool_ may be reallocated
            cur_node.children_[row[i]] = new_node_id;
            cur_node_id = new_node_id;
        } else {
            cur_node_id = it->second;
        }
    }

    pool_[cur_node_id].row_id_ = row_id;
}

void NewTrie::Delete(unsigned* row) {
    int cur_node_id = GetRootId();
    std::vector<int> node_ids;
    node_ids.push_back(cur_node_id);
    for (int i = 0; i < column_num_; ++i) {
        auto& cur_node = pool_[cur_node_id];
        auto it = cur_node.children_.find(row[i]);
        assert(it != cur_node.children_.end());
        cur_node_id = it->second;
        node_ids.push_back(cur_node_id);
    }

    // delete row_id
    pool_[cur_node_id].row_id_ = -1;

    // delete nodes if necessary
    for (int i = column_num_ - 1; i >= 0; --i) {
        int node_id = node_ids[i];
        auto& node = pool_[node_id];
        unsigned value = row[i];
        int child_id = node.children_[value];
        auto& child_node = pool_[child_id];
        if (child_node.children_.empty() && child_node.row_id_ == -1) {
            node.children_.erase(value);
        } else {
            break;
        }
    }
}