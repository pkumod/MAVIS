#include "node_materializer.h"
#include "global_vars.h"

void NodeMaterializer::MultisetIntersectionBf(std::vector<const unsigned*>& lists, std::vector<unsigned>& list_lens, std::vector<unsigned>& ret) {
    bool first = true;
    for (unsigned i = 0; i < lists.size(); ++i) {
        std::vector<unsigned> vec(lists[i], lists[i] + list_lens[i]);
        if (first) {
            ret = vec;
            first = false;
        } else {
            IntersectSortedLists(ret, vec);
        }
        if (ret.empty()) return;
    }
}

void NodeMaterializer::GetNextNodeCandidates(SuperQueryNode& sqn, unsigned cur, std::unique_ptr<unsigned[]>& embedding, std::vector<unsigned>& next_candidates) {
    unsigned su = sqn.order_[cur];
    next_candidates.clear();
    std::vector<const unsigned*> lists;
    std::vector<unsigned> list_lens;
    for (auto parent_query_node : sqn.parents_[cur]) {
        unsigned parent_dfn = sqn.pos_[parent_query_node];
        unsigned parent_data_node = embedding[parent_dfn];
        auto& cand = cs_.vcand_[parent_query_node];
        unsigned parent_data_node_idx = std::lower_bound(cand.begin(), cand.end(), parent_data_node) - cand.begin();
        if (cs_.ecand_[parent_query_node][su][parent_data_node_idx].empty()) return;
        lists.push_back(cs_.ecand_[parent_query_node][su][parent_data_node_idx].data());
        list_lens.push_back(cs_.ecand_[parent_query_node][su][parent_data_node_idx].size());
    }

    if (lists.empty()){
        next_candidates = std::vector<unsigned>(cs_.vcand_[su]);
    }
    else if (lists.size() == 1) {
        next_candidates = std::vector<unsigned>(lists[0], lists[0] + list_lens[0]);
    } else {
        MultisetIntersectionBf(lists, list_lens, next_candidates);
    }
}

void NodeMaterializer::SearchSQNEmbeddings(SuperQueryNode& sqn, SuperDataNode& sdn, unsigned cur, std::unique_ptr<unsigned[]>& embedding) {
    if (cur == sqn.GetVarNum()) {
        unsigned* new_embedding = sdn.embedding_pool_.get() + sqn.GetEmbeddingWidth() * sdn.GetEmbeddingNum();
        sdn.size_++;
        if (sdn.size_ > MAX_EMBEDDING_NUM) {
            oversized_ = true;
            return;
        }
        memcpy(new_embedding, embedding.get(), sizeof(unsigned) * sqn.GetEmbeddingWidth());
        return;
    }
    std::vector<unsigned> next_node_candidates;
    GetNextNodeCandidates(sqn, cur, embedding, next_node_candidates);
    for (auto x : next_node_candidates) {
        bool repeat = false;
        for (int i = 0; i < cur; ++i) {
            if (embedding[i] == x) {
                repeat = true;
                break;
            }
        }
        if (repeat) continue;
        embedding[cur] = x;
        SearchSQNEmbeddings(sqn, sdn, cur + 1, embedding);
        if (oversized_) return;
    }
}

bool NodeMaterializer::MaterializeSDN(unsigned sqn_id) {
    // enumerate embeddings
    SuperQueryNode& sqn = sqns_[sqn_id];
    SuperDataNode& sdn = sdns_[sqn_id];
    sdn.embedding_width_ = sqn.GetEmbeddingWidth();
    std::unique_ptr<unsigned[]> embedding = std::unique_ptr<unsigned[]>(new unsigned[sqn.GetEmbeddingWidth()]);
    long double estimate_num = (unsigned)estimator_->Estimate(sqn.node_list_);
    if (estimate_num * sqn.GetEmbeddingWidth() > 1e9) {
        oversized_ = true;
        return false;
    }
    long long allocate_size = estimate_num * sqn.GetEmbeddingWidth();
    sdn.embedding_pool_.reset(new unsigned[allocate_size]);
    for (unsigned x : cs_.vcand_[sqn.GetRootId()]) {
        embedding[0] = x;
        SearchSQNEmbeddings(sqn, sdn, 1, embedding);
        if (oversized_) {
            std::cout << "Oversized super data node found during materialization, sqn id: " << sqn_id << "\n";
            return false;
        }
    }

    // shrink the memory
    assert(estimate_num >= sdn.GetEmbeddingNum());
    auto new_size = sdn.GetEmbeddingNum() * sqn.GetEmbeddingWidth();
    unsigned* new_embedding_pool = new unsigned[new_size];
    memcpy(new_embedding_pool, sdn.embedding_pool_.get(), sizeof(unsigned) * new_size);
    sdn.embedding_pool_.reset(new_embedding_pool);

    return sdn.GetEmbeddingNum() != 0;
}

bool NodeMaterializer::MaterializeSDNs() {
    // for (unsigned i = 0; i < sqns_.size(); ++i) {
    //     std::cout << "Est " << i << ": " << estimator_->Estimate(sqns_[i].node_list_) << std::endl;
    // }
    for (unsigned i = 0; i < sqns_.size(); ++i) {
        unsigned sqn_id = i;
        if (!MaterializeSDN(sqn_id)) return false;
        // std::cout << sqn_id << " materialized, the number of embeddings: " << sdns_[sqn_id].GetEmbeddingNum() << std::endl;
    }
    return true;
}