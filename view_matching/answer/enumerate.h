#ifndef ENUMERATE_INFO_H
#define ENUMERATE_INFO_H

#include <fstream>
#include <map>

#include "graph/graph.h"
#include "multilayer_trie.h"
#include "pretty_print.h"
#include "relation/catalog.h"
#include "rewrite_query.h"
#include "BitSet.h"
#include "view/storage/mat_view.h"
#include "combination.h"
#include "global_vars.h"

#include "auxilary_info.h"

#include "sparse_bitset.h"


class EnumerateState {
   public:
    int cur_depth_;
    int cur_offset_;
    std::vector<unsigned> idx_embedding_;
    std::vector<unsigned> sdn_embedding_ids_;
    std::vector<int> vis_;

    std::unique_ptr<SparseMap> vis1_;

    std::vector<unsigned> anc_;
    std::vector<unsigned> fs_;

    std::vector<std::unique_ptr<unsigned[]>> candidate_buffers_, candidate_temps_;
    std::vector<unsigned> candidate_buffer_size_;

    std::unique_ptr<unsigned[]> embedding_;
    long double embedding_cnt_;
    long double product_;

    long long state_cnt_ = 0;
    long long fail_state_cnt_ = 0;
    std::vector<long long> intersection_cost_;
    std::vector<long long> state_cnt1_;
    std::vector<long long> fail_state_cnt1_;

    std::vector<std::unique_ptr<unsigned[]>> trie_bufs_;

    bool finished_ = false;

    void Print(bool flag = true) {
        std::cout << "d" << cur_depth_ << ",f" << cur_offset_ << ",e" << embedding_cnt_ << ",s" << state_cnt_ << ",ss" << state_cnt1_[cur_offset_] << ",p" << product_ << ": ";
        for (int i = 0; i < cur_offset_; ++i) {
            std::cout << embedding_[i] << (i == cur_offset_ - 1 ? "" : ",");
        }
        if (flag) std::cout << std::endl;
    }

    void Update1(int idx_candidate, unsigned candidate, unsigned query_node) {
        idx_embedding_[cur_offset_] = idx_candidate;
        embedding_[cur_offset_] = candidate;
        cur_depth_++;
        cur_offset_++;
        // vis_[candidate] = query_node;
        vis1_->Set(candidate, query_node);
    }
    void Restore1() {
        auto candidate = embedding_[cur_offset_ - 1];
        // vis_[candidate] = -1;
        vis1_->Reset(candidate);
        cur_offset_--;
        cur_depth_--;
    }
    void UpdateK(unsigned* embedding, unsigned embedding_width, int rw_node_id, RewriteQuery::Node* mat_node, int i) {
        for (int j = 0; j < embedding_width; ++j) {
            // vis_[embedding[j]] = mat_node->GetInnerOrder()[j];
            vis1_->Set(embedding[j], mat_node->GetInnerOrder()[j]);
            embedding_[cur_offset_ + j] = embedding[j];
            idx_embedding_[cur_offset_ + j] = mat_node->encoded_embeddings_[i][j];
        }
        sdn_embedding_ids_[rw_node_id] = i;
        cur_depth_++;
        cur_offset_ += embedding_width;
    }
    void RestoreK(unsigned embedding_width) {
        for (int j = 0; j < embedding_width; ++j) {
            // vis_[embedding_[cur_offset_ - 1 - j]] = -1;
            vis1_->Reset(embedding_[cur_offset_ - 1 - j]);
        }
        cur_offset_ -= embedding_width;
        cur_depth_--;
    }
};

class Enumerator {
    RewriteQuery rq_;
    AuxilaryInfo aux_;

    std::unique_ptr<MyBitSet> intersection_bs_, check_edge_bs_; 
    std::vector<std::vector<ui>> lcs_, tmp_lcs_;

    std::vector<std::vector<ui>> sn_cand_sets_;

    ui calc_encode_count_ = 0;
    ui check_vis_count_ = 0;
    ui tail_conflict_count_ = 0;
    ui get_local_candidate_count_ = 0;
    long long conflict_count_ = 0;
    long long empty_count_ = 0;

    std::vector<ui> non_port_order_, is_shell_;
    std::vector<ui> simple_failing_set_;

   public:
    Enumerator(RewriteQuery&& rq1) : rq_(std::move(rq1)) {}

    void CheckLC(EnumerateState& state, unsigned& last_fs, std::vector<ui>& lc, RewriteQuery::Node* mat_node, int embedding_width, ui rw_node_id);

    bool CheckEmbedding(ui *embedding, ui len);

    void UpdateFailingSetByConflictClass(EnumerateState& state, unsigned u, unsigned v, unsigned& last_fs);

    bool CheckPatchEdges(RewriteQuery::Node* mat_node, unsigned dst_embedding_id);

    void CalcEncodeEmbedding(RewriteQuery::Node* mat_node, unsigned dst_embedding_id);

    void GetLocalCandidate(EnumerateState& state, unsigned query_node);

    void EnumerateOnTrie(int cur, int trie_id, int last_not_empty, unsigned& last_fs, ui need_check_vis,
                         int embedding_width, NewTrie* trie, const std::vector<unsigned>& order, EnumerateState& state, std::vector<ui>& lc);
    bool HandleMatNode(EnumerateState& state, unsigned int& last_fs);
    bool EnumerateMatNode(EnumerateState& state, unsigned& last_fs);
    bool HandleNormalNode(EnumerateState& state, unsigned int& last_fs);
    bool HandleSingleNode(ui rw_node_id, EnumerateState& state, ui& last_fs, ui& cur_fs, ui sum, ui query_node, std::vector<ui>& super_node_candidates);
    bool EnumerateDfs(EnumerateState& state, unsigned& last_fs);


    long double EnumerateFromRewriteQuery(Graph* query_graph, Graph* data_graph, long long num_limit, unsigned* embeddings);
    void Statistic(EnumerateState& state);
    void Initialize(EnumerateState& state);
    long double ExecuteWithinTimeLimit(Graph* query_graph, Graph* data_graph, long long num_limit, unsigned time_limit, unsigned* embeddings);

    bool EnumerateOnNonPortVertices(EnumerateState& state, ui cur);
};

#endif  // ENUMERATE_INFO_H
