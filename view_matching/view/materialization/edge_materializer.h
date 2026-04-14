#ifndef EDGE_MATERIALIZER_H
#define EDGE_MATERIALIZER_H

#include "mat_view.h"
#include "multilayer_trie.h"

class EdgeMaterializer {
    friend class Materializer;
    SuperQueryGraph& sqg_;
    std::vector<SuperDataNode>& sdns_;
    std::vector<std::vector<SuperDataEdge>>& ses_;
    CandidateSpace& cs_;
    Graph* data_graph_;

    std::vector<int> idxs_;

    // statistics
    struct Stats {
        ui src_embedding_num{};
        ui src_group_num{};
        ui dst_embedding_num{};
        ui dst_group_num{};
        bool src_all_materialized{};
        bool dst_all_materialized{};
        ui src_linked_group_num{};
        ui dst_linked_group_num{};
    };
    std::vector<std::vector<Stats>> stats_;
    unsigned long long get_cand_cost_{0};
    unsigned long long intersect_cost_{0};
    unsigned long long intersect_size_{0};

   public:
    EdgeMaterializer(SuperQueryGraph& sqg, std::vector<SuperDataNode>& sdns, std::vector<std::vector<SuperDataEdge>>& ses, Graph* data_graph, CandidateSpace& cs)
        : sqg_(sqg), sdns_(sdns), ses_(ses), data_graph_(data_graph), cs_(cs) {}

   private:
    void GetKeyPosition(unsigned su, unsigned sv, std::vector<int>& kp, std::vector<int>& kv);
    void GetKeyLink(unsigned su, unsigned sv, std::vector<int>& su_kv, std::vector<int>& sv_kv, std::vector<std::vector<int>>& klk);
    void GroupEmbeddings(unsigned su, unsigned sv, const std::vector<int>& kp, std::map<std::vector<int>, unsigned >& k2g, std::vector<std::vector<int>>& g2k, SuperDataEdge::Index& index);
    void BuildTrie(std::map<std::vector<int>, unsigned >& k2g, std::vector<int>& kp, MultiLayerTrie*& mlt);
    auto GetCandidateSets(int sv_key_len, std::vector<std::vector<int>>& sv_klk, std::vector<int>& su_kv, std::vector<int>& sv_kv, std::vector<int>& key);
    std::vector<int> LinkOneGroup2NGroups(std::vector<int>& key, int sv_key_len, std::vector<std::vector<int>>& sv_klk, std::vector<int>& su_kv, std::vector<int>& sv_kv, MultiLayerTrie* mlt);
    void LinkG2GEdge(unsigned su, unsigned sv, std::vector<int>& su_kp,
                     std::vector<int>& sv_kp, std::vector<int>& su_kv,
                     std::vector<int>& sv_kv,
                     std::vector<std::vector<int>>& sv_klk,
                     std::vector<std::vector<unsigned >>& su_g2k,
                     std::map<std::vector<int>, unsigned >& sv_k2g,
                     std::vector<std::vector<int>>& link,
                     std::vector<bool>& materialized);
    bool NoRepeat(unsigned su, unsigned sv, int u_e, int v_e);
    bool CheckEdgeBF(unsigned su, unsigned sv, int u_e, int v_e);
    bool CheckEdge1(unsigned su, unsigned sv, int u_e, int v_e);
    bool CheckNewSuperEdge(unsigned su, unsigned sv);
    void LinkSuperEdge(unsigned su, unsigned sv, bool is_td);

   public:
    void LinkSuperEdge(bool is_td = false);
};

#endif