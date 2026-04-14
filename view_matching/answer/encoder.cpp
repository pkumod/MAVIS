#include "encoder.h"

#include "primitive/projection.h"

void Encoder::execute() {
    join_plan_ = aux_.query_node_order_.data();
    auto storage = aux_.storage_.get();
    auto start = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 1; i < aux_.query_graph_->getVerticesCount(); ++i) {
        uint32_t u = join_plan_[i];
        if (!aux_.first_of_nec_[u]) {
            continue;
        }

        for (uint32_t j = 0; j < i; ++j) {
            uint32_t bn = join_plan_[j];
            if (aux_.query_graph_->checkEdgeExistence(bn, u) && rq_.BelongToWhichRWNode(bn) != rq_.BelongToWhichRWNode(u)) {
                convert_to_encoded_relation(storage, bn, u);
                aux_.need_idx_ |= (1ULL << bn);
            }
        }
    }

    // for (ui i = 0; i < aux_.query_graph_->getVerticesCount(); ++i) {
    //     if (aux_.need_idx_ & (1ULL << i) && storage->reverse_index_built(i) == false) {
    //         auto start = std::chrono::high_resolution_clock::now();
    //         storage->build_reverse_index(i);
    //         auto end = std::chrono::high_resolution_clock::now();
    //         std::cout << "build reverse index for " << i << " time (ms): "
    //                   << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1e6 << std::endl;
    //     }
    // }
    auto end = std::chrono::high_resolution_clock::now();
    encoding_time_ = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

void Encoder::convert_to_encoded_relation(catalog* storage, uint32_t u, uint32_t v) {
    uint32_t src = std::min(u, v);
    uint32_t dst = std::max(u, v);
    edge_relation& target_edge_relation = storage->edge_relations_[src][dst];
    Edge* edges = target_edge_relation.edges_;
    uint32_t edge_size = target_edge_relation.size_;
    assert(edge_size > 0);

    // if (storage->reverse_index_built(v) == false) {
    //     auto start = std::chrono::high_resolution_clock::now();
    //     storage->build_reverse_index(v);
    //     auto end = std::chrono::high_resolution_clock::now();
    //     std::cout << "build reverse index for " << v << " time (ms): "
    //               << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1e6 << std::endl;
    // }

    uint32_t v_candidates_cnt = storage->get_num_candidates(v);
    uint32_t* v_candidates = storage->get_candidates(v);

    for (uint32_t i = 0; i < v_candidates_cnt; ++i) {
        uint32_t v_candidate = v_candidates[i];
        temp_buffer1_[v_candidate] = i + 1;
    }

    uint32_t u_p = 0;
    uint32_t v_p = 1;
    if (u > v) {
        // Sort R(v, u) by u.
        std::sort(edges, edges + edge_size, [](Edge& l, Edge& r) -> bool {
            if (l.vertices_[1] == r.vertices_[1])
                return l.vertices_[0] < r.vertices_[0];
            return l.vertices_[1] < r.vertices_[1];
        });
        u_p = 1;
        v_p = 0;
    }

    encoded_trie_relation& target_encoded_trie_relation = storage->encoded_trie_relations_[u][v];
    uint32_t edge_cnt = edge_size;
    uint32_t u_candidates_cnt = storage->get_num_candidates(u);
    uint32_t* u_candidates = storage->get_candidates(u);
    target_encoded_trie_relation.size_ = u_candidates_cnt;
    target_encoded_trie_relation.offset_ = new uint32_t[u_candidates_cnt + 1];
    target_encoded_trie_relation.children_ = new uint32_t[edge_size];

    uint32_t offset = 0;
    uint32_t edge_index = 0;

    for (uint32_t i = 0; i < u_candidates_cnt; ++i) {
        uint32_t u_candidate = u_candidates[i];
        target_encoded_trie_relation.offset_[i] = offset;
        uint32_t local_degree = 0;
        while (edge_index < edge_cnt) {
            uint32_t u0 = edges[edge_index].vertices_[u_p];
            uint32_t v0 = edges[edge_index].vertices_[v_p];
            if (u0 == u_candidate) {
                // if (storage->get_candidate_index(v, v0) < storage->get_num_candidates(v)) {
                //     target_encoded_trie_relation.children_[offset + local_degree] = v0;
                //     local_degree += 1;
                // }
                if (temp_buffer1_[v0] > 0) {
                    target_encoded_trie_relation.children_[offset + local_degree] = v0;
                    local_degree += 1;
                }
            } else if (u0 > u_candidate) {
                break;
            }

            edge_index += 1;
        }

        offset += local_degree;

        if (local_degree > target_encoded_trie_relation.max_degree_) {
            target_encoded_trie_relation.max_degree_ = local_degree;
        }
    }

    target_encoded_trie_relation.offset_[u_candidates_cnt] = offset;

    for (uint32_t i = 0; i < v_candidates_cnt; ++i) {
        uint32_t v_candidate = v_candidates[i];
        temp_buffer1_[v_candidate] = 0;
    }
}
