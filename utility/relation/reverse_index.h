#pragma once

#include <vector>
#include <memory>
#include <cmath>
#include <cstring>

class CompressReverseIndex {
public:
    unsigned n_; 
    unsigned width_; 
    std::vector<std::unique_ptr<int[]>> blocks_;
    ui len_;

    CompressReverseIndex(unsigned n, ui* arr, ui len): n_(n), len_(len) {
        width_ = 0;
        while ((1U << width_) < sqrt(n)) {
            width_++;
        }
        
        blocks_.resize((n_ >> width_) + 1);
        for (unsigned i = 0; i < blocks_.size(); ++i) {
            blocks_[i] = nullptr;
        }
        for (int i = 0; i < len; ++i) {
            auto v = arr[i];
            int block_id = v >> width_;
            int offset = v & ((1U << width_) - 1);
            if (blocks_[block_id] == nullptr) {
                blocks_[block_id] = std::make_unique<int[]>(1 << width_);
                memset(blocks_[block_id].get(), 0, sizeof(int) * (1 << width_));
            }
            blocks_[block_id][offset] = i + 1;
        }
    }

    int Get(unsigned v) {
        int block_id = v >> width_;
        int offset = v & ((1U << width_) - 1);
        if (blocks_[block_id] == nullptr) {
            return len_;
        }
        return blocks_[block_id][offset] == 0 ? len_ : blocks_[block_id][offset] - 1;
    }
};

class ReverseIndex {
public:
    unsigned n_; 
    int len_;
    std::unique_ptr<int[]> index_;

    ReverseIndex(unsigned n, ui* arr, ui len): n_(n), len_(len) {
        index_ = std::make_unique<int[]>(n_);
        memset(index_.get(), -1, sizeof(int) * n_);
        for (int i = 0; i < len; ++i) {
            index_[arr[i]] = i;
        }
    }

    int Get(unsigned v) {
        return index_[v] == -1 ? len_ : index_[v];
    }
};