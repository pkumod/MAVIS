#pragma once

#include <vector>
#include <cassert>

// the sparse map divided into 64 blocks
// sketch_ records which blocks are non-empty

class SparseMap {
public:    
    unsigned n_;
    unsigned long long sketch_;
    unsigned block_size_;
    unsigned width_;
    std::vector<int> data_[64];
    int count_[64]; 

    SparseMap(unsigned n): n_(n), sketch_(0) {
        auto limit = (n + 63) / 64;
        block_size_ = 1;
        width_ = 0;
        while (block_size_ < limit) {
            block_size_ *= 2;
            width_++;
        }

        for (int i = 0; i < 64; ++i) {
            data_[i].resize(block_size_, -1);
            data_[i].assign(block_size_, -1);
            count_[i] = 0;
        }
    }

    void Set(unsigned i, int val) {
        int block = i >> width_;
        int pos = i & (block_size_ - 1);
        assert(data_[block][pos] == -1);
        data_[block][pos] = val;
        count_[block]++;
        if (count_[block] == 1) {
            sketch_ |= (1ULL << block);
        }
    }

    void Reset(unsigned i) {
        int block = i >> width_;
        int pos = i & (block_size_ - 1);
        assert(data_[block][pos] != -1);
        data_[block][pos] = -1;
        count_[block]--;
        if (count_[block] == 0) {
            sketch_ &= ~(1ULL << block);
        }
    }

    int Get(unsigned i) {
        int block = i >> width_;
        if ((sketch_ & (1ULL << block)) == 0) return -1;
        int pos = i & (block_size_ - 1);
        return data_[block][pos];
    }
};