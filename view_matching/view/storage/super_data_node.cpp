#include "super_data_node.h"
#include <cstring>

void SuperDataNode::Dump(std::ofstream& fout) {
    fout << GetEmbeddingNum() << std::endl;
    fout << embedding_width_ << std::endl;
    for (int i = 0; i < GetEmbeddingNum(); ++i) {
        for (int j = 0; j < embedding_width_; ++j) {
            fout << GetEmbedding(i)[j] << " ";
        }
    }
    fout << std::endl;
}

void SuperDataNode::Load(std::ifstream& fin) {
    fin >> size_;
    fin >> embedding_width_;
    embedding_pool_.reset(new unsigned[size_ * embedding_width_]);
    for (int i = 0; i < size_; ++i) {
        unsigned* embedding = GetEmbedding(i);
        for (int j = 0; j < embedding_width_; ++j) {
            fin >> embedding[j];
        }
    }
}

void SuperDataNode::BuildTrie(bool incremental) {
    trie_.reset(new NewTrie(embedding_width_, incremental));
    for (int i = 0; i < size_; ++i) {
        trie_->Insert(GetEmbedding(i), i);
    }
}

int SuperDataNode::InsertEmbedding(unsigned* embedding) {
    // expand pool if needed
    if (capacity_ < size_ + 1) {
        capacity_ = size_ * 2;
        std::unique_ptr<unsigned[]> new_embedding_pool(new unsigned[capacity_ * embedding_width_]);
        memcpy(new_embedding_pool.get(), embedding_pool_.get(), size_ * embedding_width_ * sizeof(unsigned));
        embedding_pool_.reset(new_embedding_pool.release());
    }
    // insert embedding to pool
    memcpy(GetEmbedding(size_), embedding, embedding_width_ * sizeof(unsigned));
    size_++;
    // insert embedding to trie
    trie_->Insert(embedding, size_ - 1);
    return size_ - 1;
}

void SuperDataNode::DeleteEmbedding(ui embedding_id) {
    // delete from trie
    ui* embedding = GetEmbedding(embedding_id);
   trie_->Delete(embedding);

    // just clear the embedding in pool, do not move others
    memset(embedding, -1, embedding_width_ * sizeof(unsigned));
}

bool SuperDataNode::CheckEmbedding(ui embedding_id) {
    ui* embedding = GetEmbedding(embedding_id);
    return embedding[0] != (unsigned)(-1);
}

ui* SuperDataNode::GetEmbedding(int embedding_id) {
    return embedding_pool_.get() + embedding_id * embedding_width_;
}

unsigned long long SuperDataNode::MemoryCost() {
    auto embedding_cost = GetEmbeddingNum() * embedding_width_ * sizeof(unsigned);
    auto meta_cost = 2 * sizeof(unsigned);  // embedding_width_ and size_
    auto trie_cost = trie_->GetMemoryCost();
    return embedding_cost + meta_cost + trie_cost;
}