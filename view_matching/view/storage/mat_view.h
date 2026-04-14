#pragma once

#include <algorithm>
#include <unordered_map>

#include "types.h"
#include "graph.h"
#include "super_query_graph.h"
#include "subgraph_filter.h"
#include "multilayer_trie.h"
#include "super_data_node.h"
#include "super_data_edge.h"

SuperDataEdgeView GetSuperDataEdgeView(int su, int sv, std::vector<std::vector<SuperDataEdge>>& ses);

class MatView {
   public:
    TreePartitionGraph sqg_;
    std::vector<SuperDataNode> sdns_;
    std::vector<std::vector<SuperDataEdge>> ses_;

    CandidateSpace cs_;

    std::string file_name_;

    bool loaded_ = true;

    void DumpSuperEdge(std::ofstream& fout);
    void Dump(std::string file_name);

    void LoadSuperEdge(std::ifstream& fin);
    void LoadSQG();
    void Load();

    void Release();

    void BuildTries(bool incremental) { 
        for (auto& sdn : sdns_) sdn.BuildTrie(incremental); 
    }

    SuperDataEdgeView GetSuperDataEdgeView(int su, int sv) {
        return ::GetSuperDataEdgeView(su, sv, ses_);
    }

   private:
    void PrintSE(int su, int sv);

   public:
    void Print();
    unsigned long long MemoryCost();
};


