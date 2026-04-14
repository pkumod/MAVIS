//
// Created by ssunah on 6/22/18.
//

#ifndef SUBGRAPHMATCHING_GRAPH_H
#define SUBGRAPHMATCHING_GRAPH_H

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <map>
#include <iostream>
#include <fstream>
#include <vector>
#include "utility/sparsepp/spp.h"
#include "configuration/types.h"
#include "configuration/config.h"

class FastReader {
   public:
    char buf[1 << 20], *p1 = buf, *p2 = buf;
    FILE *fp;
    
    FastReader(const std::string& file_path) {
        fp = fopen(file_path.c_str(), "r");
    }

    char gc() {
        return (p1 == p2 && (p2 = (p1 = buf) + fread(buf, 1, sizeof(buf), fp), p1 == p2) ? EOF : *p1++);
    }

    char get_alpha() {
        char ch = gc();
        while (!isalpha(ch) && ch != EOF) {
            ch = gc();
        }
        return ch;
    }

    unsigned get_unsigned() {
        char ch = gc();
        while (!isdigit(ch) && ch != EOF) {
            ch = gc();
        }
        ui ret = 0;
        while (isdigit(ch)) {
            ret = ret * 10 + ch - '0';
            ch = gc();
        }
        return ret;
    }

    ~FastReader() {
        fclose(fp);
    }
};

/**
 * A graph is stored as the CSR format.
 */
using spp::sparse_hash_map;
class Graph {
private:
    bool enable_label_offset_;

    ui vertices_count_;
    ui edges_count_;
    

    std::vector<std::vector<ui>> neighbors_;
    std::vector<LabelID> labels_;
    std::map<LabelID, std::vector<ui>> reverse_index_; // label -> vertex_ids
    std::map<LabelID, ui> labels_frequency_;

    sparse_hash_map<uint64_t, std::vector<Edge>> edge_index1_; // immutable
#if OPTIMIZED_LABELED_GRAPH == 1
    std::vector<std::vector<std::pair<uint32_t, uint32_t>>> nlf_;
#endif

public:
    // core information, only used for query graph, therefore, there is no need
    // to support incremental update
    int* core_table_; // core value of each vertex
    ui core_length_; // 2-core size
    bool enable_edge_index_;


private:
    void BuildReverseIndex();

#if OPTIMIZED_LABELED_GRAPH == 1
    void BuildNLF();
#endif

public:
    Graph(const bool enable_label_offset, bool enable_edge_index = true) {
        enable_label_offset_ = enable_label_offset;
        enable_edge_index_ = enable_edge_index;

        vertices_count_ = 0;
        edges_count_ = 0;
        core_length_ = 0;

        core_table_ = nullptr;
        labels_frequency_.clear();
    }

    ~Graph() {
        delete[] core_table_;
    }

public:
    void loadGraphFromIStream(std::istream& in);
    void loadGraphFromFile(const std::string& file_path);
    void loadGraphWithoutMeta(const std::string& file_path);
    void printGraphMetaData();
    void loadGraphFromTuples(
        const std::vector<std::tuple<VertexID, LabelID, ui>>& v_tuples,
        const std::vector<std::tuple<VertexID, VertexID>>& e_tuples);

    void dumpCompressedGraph(const std::string& file_path);
    void loadCompressedGraph(const std::string& file_path);

    void buildIndex();

   public:
    const ui getLabelsCount() const {
        return labels_frequency_.size();
    }

    const ui getVerticesCount() const {
        return vertices_count_;
    }

    const ui getEdgesCount() const {
        return edges_count_;
    }

    const ui getGraphMaxDegree() const {
        auto max_degree = 0;
        for (int i = 0; i < vertices_count_; ++i) {
            max_degree = std::max(max_degree, (int)neighbors_[i].size());
        }
        return max_degree;
    }

    const ui getGraphMaxLabelFrequency() const {
        ui res = 0;
        for (auto element : labels_frequency_) {
            res = std::max(res, element.second);
        }
        return res;
    }

    const ui getVertexDegree(const VertexID id) const {
        return neighbors_[id].size();
    }

    const ui getLabelsFrequency(const LabelID label) const {
        return labels_frequency_.find(label) == labels_frequency_.end() ? 0 : labels_frequency_.at(label);
    }

    const ui getCoreValue(const VertexID id) const {
        return core_table_[id];
    }

    const ui get2CoreSize() const {
        return core_length_;
    }
    const LabelID getVertexLabel(const VertexID id) const {
        return labels_[id];
    }

    const ui * getVertexNeighbors(const VertexID id, ui& count) const {
        count = neighbors_[id].size();
        return neighbors_[id].data();
    }

    std::vector<ui> getNeighborVec(const VertexID id) {
        ui len;
        const ui* neighbors = getVertexNeighbors(id, len);
        return std::vector<ui>(neighbors, neighbors + len);
    }

    const ui getMaxLabelId() const {
        return labels_frequency_.rbegin()->first + 1;
    }

    const sparse_hash_map<uint64_t, std::vector<Edge>>* getEdgeIndex2() const {
        assert(enable_edge_index_);
        return &edge_index1_;
    }

    const ui * getVerticesByLabel(const LabelID id, ui& count) const {
        auto iter = reverse_index_.find(id);
        count = iter == reverse_index_.end() ? 0 : iter->second.size();
        return count == 0 ? nullptr : iter->second.data();
    }

#if OPTIMIZED_LABELED_GRAPH == 1

    const std::vector<std::pair<uint32_t, uint32_t>>* getVertexNLF(const VertexID id) const {
        return &nlf_[id];
    }
#endif

    bool checkEdgeExistence(VertexID u, VertexID v) const {
        if (getVertexDegree(u) < getVertexDegree(v)) {
            std::swap(u, v);
        }
        ui count = 0;
        const VertexID* neighbors =  getVertexNeighbors(v, count);

        int begin = 0;
        int end = count - 1;
        while (begin <= end) {
            int mid = begin + ((end - begin) >> 1);
            if (neighbors[mid] == u) {
                return true;
            }
            else if (neighbors[mid] > u)
                end = mid - 1;
            else
                begin = mid + 1;
        }

        return false;
    }

    void buildCoreTable();

    void buildEdgeIndex();

    void PrintDot(std::ofstream& fout);

    void Dump(std::ofstream& out);
    void Dump(std::string file_name);

    void GetTupleFormat(std::vector<std::tuple<VertexID, LabelID, ui>>& v_tuples,
                        std::vector<std::tuple<VertexID, VertexID>>& e_tuples);
    
    Graph* Clone();

    unsigned long long getMemoryCost() {
        unsigned long long cost = 0;
        // neighbors_
        for (ui i = 0; i < vertices_count_; ++i) {
            cost += neighbors_[i].size() * sizeof(ui);
        }
        // labels_
        cost += labels_.size() * sizeof(LabelID);
        return cost;
    }

    void OutputGR(std::string file_path) {
        std::ofstream fout;
        fout.open(file_path);
        if (!fout.is_open()) {
            std::cerr << "Error opening file: " << file_path << std::endl;
            return;
        }
        fout << "p tw " << vertices_count_ << " " << edges_count_ << "\n";
        for (ui i = 0; i < vertices_count_; ++i) {
            ui len;
            const ui* nbrs = getVertexNeighbors(i, len);
            for (ui j = 0; j < len; ++j) {
                if (i < nbrs[j]) {
                    fout << i + 1 << " " << nbrs[j] + 1 << "\n";
                }
            }
        }
        fout.close();
    }

    void InsertEdge(VertexID u, VertexID v);

    void DeleteEdge(VertexID u, VertexID v);
};

#define ITERATE_NBRS(graph, u, v)                        \
    auto _count_ = ui(0);                                \
    auto _nbrs_ = graph->getVertexNeighbors(u, _count_); \
    for (ui _i_ = 0, v = _nbrs_[0];                    \
         _i_ < _count_;                                  \
         v = _nbrs_[++_i_])

#endif //SUBGRAPHMATCHING_GRAPH_H
