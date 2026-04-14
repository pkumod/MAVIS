#pragma once

#include <memory>
#include <vector>
#include <fstream>
#include <iostream>
#include "types.h"
#include "multilayer_trie.h"

class SuperDataNode {
   public:
    unsigned embedding_width_;
    unsigned long long size_{}, capacity_{};
    std::unique_ptr<unsigned[]> embedding_pool_;

    std::unique_ptr<NewTrie> trie_;

    std::vector<bool> complete_;

    unsigned long long GetEmbeddingNum() { return size_; }

    void BuildTrie(bool incremental);

    void Dump(std::ofstream& fout);
    void Load(std::ifstream& fin);

   public:
    SuperDataNode() = default;
    SuperDataNode(const SuperDataNode& other) = delete;
    SuperDataNode(SuperDataNode&& other) = default;
    SuperDataNode& operator=(const SuperDataNode& other) = delete;
    SuperDataNode& operator=(SuperDataNode&& other) = default;
    ~SuperDataNode() = default;

    ui* GetEmbedding(int embedding_id);
    unsigned long long MemoryCost();

    void DeleteEmbedding(ui embedding_id);
    int InsertEmbedding(unsigned* embedding);
    int GetEmbeddingId(unsigned* embedding) { return trie_->GetRowId(embedding); }
    bool CheckEmbedding(ui embedding_id);
    void ReleaseComplete() { std::vector<bool>().swap(complete_); }
};