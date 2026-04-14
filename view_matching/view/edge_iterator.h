#ifndef EDGE_ITERATOR_H
#define EDGE_ITERATOR_H

#include "mat_view.h"

class EdgeIterator {
    SuperDataEdge::Index& src_index_;
    SuperDataEdge::Index& dst_index_;
    std::vector<int>& link_;

    int group_off_;
    int embedding_off_;
    int last = -1;

   public:

    EdgeIterator(SuperDataEdge& se, bool d, ui src_embedding_id)
        : src_index_(d ? se.sv_index_ : se.su_index_),
          dst_index_(d ? se.su_index_ : se.sv_index_),
          link_(se.linked_groups_[d][src_index_.embedding_id2group_id_[src_embedding_id]]) {
        Init();
    }

    EdgeIterator& operator=(const EdgeIterator& other) {
        src_index_ = other.src_index_;
        dst_index_ = other.dst_index_;
        link_ = other.link_;
        group_off_ = other.group_off_;
        embedding_off_ = other.embedding_off_;
        return *this;
    }

    void Init() {
        group_off_ = 0;
        embedding_off_ = 0;
    }

    auto& GetEmbeddingIds() {
        auto group_id = link_[group_off_];
        return dst_index_.group_id2embedding_ids_[group_id];
    }

    void Next() {
        embedding_off_++;
        if (embedding_off_ >= GetEmbeddingIds().size()) {
            group_off_++;
            embedding_off_ = 0;
        }
    }

    bool End() { return group_off_ >= link_.size(); }

    int GetVal() {
        auto embedding_id = GetEmbeddingIds()[embedding_off_];
        return embedding_id;
    }
};

template <typename T>
class ExposedPriorityQueue : public std::priority_queue<T> {
   public:
    T GetSecond() {
        assert(this->size() >= 2);
        auto ret = this->c[1];
        if (this->size() >= 3 && this->comp(ret, this->c[2])) {
            ret = this->c[2];
        }
        return ret;
    }
};

class OrderedEdgeIterator {
    SuperDataEdge::Index& src_index_;
    SuperDataEdge::Index& dst_index_;
    std::vector<int>& link_;

    // ExposedPriorityQueue<std::tuple<int, int, int>> pq_;  // {embedding_id, group_id, group_off}
    std::priority_queue<std::tuple<int, int, int>, std::vector<std::tuple<int, int, int>>, std::greater<std::tuple<int, int, int>>> pq_;
   public:
    OrderedEdgeIterator(SuperDataEdge& se, bool d, int src_embedding_id)
        : src_index_(d ? se.sv_index_ : se.su_index_),
          dst_index_(d ? se.su_index_ : se.sv_index_),
          link_(se.linked_groups_[d][src_index_.embedding_id2group_id_[src_embedding_id]]) {}

    void Init() {
        for (auto group_id : link_) {
            auto& embedding_ids = dst_index_.group_id2embedding_ids_[group_id];
            if (embedding_ids.size() > 0) {
                pq_.push({embedding_ids[0], group_id, 0});
            }
        }
    }

    void Next() {
        auto [embedding_id, group_id, group_off] = pq_.top();
        pq_.pop();
        group_off++;
        if (group_off < dst_index_.group_id2embedding_ids_[group_id].size()) {
            pq_.push({dst_index_.group_id2embedding_ids_[group_id][group_off], group_id, group_off});
        }
    }

    bool End() {
        return pq_.empty();
    }

    int GetVal() {
        auto [embedding_id, group_id, group_off] = pq_.top();
        return embedding_id;
    }

    // int PeekNext() {
    //     if (pq_.size() >= 2) {
    //         auto [embedding_id, group_id, group_off] = pq_.GetSecond();
    //         return embedding_id;
    //     }
    //     return -1;
    // }
};

#endif