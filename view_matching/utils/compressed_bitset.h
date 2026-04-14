#pragma once

#include <cassert>
#include <cmath>
#include <memory>
#include <vector>

class CompressedBitSet {
   public:
    int n_{};
    int width_{};
    std::vector<std::vector<bool>> blocks_;

    CompressedBitSet(int n) : n_(n) {
        int sqrt_n = static_cast<int>(std::sqrt(n));
        width_ = 0;
        while ((1 << width_) < sqrt_n) {
            width_++;
        }
        int block_size = 1 << width_;
        int num_blocks = (n + block_size - 1) / block_size;
        blocks_.resize(num_blocks);
    }

    auto GetIndex(int pos) const {
        int block_index = pos >> width_;
        int bit_index = pos & ((1 << width_) - 1);
        assert(block_index < blocks_.size());
        return std::make_pair(block_index, bit_index);
    }

    void Set(int pos) {
        auto [block_index, bit_index] = GetIndex(pos);
        if (blocks_[block_index].empty()) {
            blocks_[block_index].resize(1 << width_, false);
        }
        blocks_[block_index][bit_index] = true;
    }

    bool Test(int pos) const {
        auto [block_index, bit_index] = GetIndex(pos);
        if (blocks_[block_index].empty()) {
            return false;
        }
        return blocks_[block_index][bit_index];
    }
};