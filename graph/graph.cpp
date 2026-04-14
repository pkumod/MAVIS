//
// Created by ssunah on 6/22/18.
//

#include "graph.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <vector>

#include "utility/graphoperations.h"

void Graph::BuildReverseIndex() {
    for (ui i = 0; i < vertices_count_; ++i) {
        LabelID label = labels_[i];
        reverse_index_[label].push_back(i);
    }
}

#if OPTIMIZED_LABELED_GRAPH == 1
void Graph::BuildNLF() {
    nlf_.resize(vertices_count_);
    for (ui i = 0; i < vertices_count_; ++i) {
        ui count;
        const VertexID* neighbors = getVertexNeighbors(i, count);
        std::map<LabelID, ui> temp_nlf;
        for (ui j = 0; j < count; ++j) {
            VertexID u = neighbors[j];
            LabelID label = getVertexLabel(u);
            temp_nlf[label] += 1;
        }
        for (auto element : temp_nlf) {
            nlf_[i].emplace_back(element);
        }
    }
}

#endif

void Graph::buildIndex() {
    bool big_graph = vertices_count_ > 5000;

    BuildReverseIndex();
    if (big_graph) std::cout << "finished building reverse index." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    if (enable_edge_index_) {
        buildEdgeIndex();
    }
    auto end = std::chrono::high_resolution_clock::now();
    if (big_graph) std::cout << "build edge index cost: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
    if (big_graph) std::cout << "finished building edge index." << std::endl;

    start = std::chrono::high_resolution_clock::now();
    for (ui i = 0; i < vertices_count_; ++i) {
        std::sort(neighbors_[i].begin(), neighbors_[i].end());
    }
    end = std::chrono::high_resolution_clock::now();
    if (big_graph) std::cout << "sort neighbors cost: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;

#if OPTIMIZED_LABELED_GRAPH == 1
    if (enable_label_offset_) {
        auto start_nlf = std::chrono::high_resolution_clock::now();
        BuildNLF();
        auto end_nlf = std::chrono::high_resolution_clock::now();
        if (big_graph) std::cout << "build NLF cost: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_nlf - start_nlf).count() << " ms" << std::endl;
        if (big_graph) std::cout << "finished building NLF." << std::endl;
    }
#endif
}

void Graph::loadGraphFromIStream(std::istream& infile) {
    char type;
    bool big_graph = false;
    auto start = std::chrono::high_resolution_clock::now();
    infile >> type >> vertices_count_ >> edges_count_;
    big_graph = vertices_count_ > 5000;
    neighbors_.resize(vertices_count_);
    labels_.resize(vertices_count_);
    for (int i = 0; i < vertices_count_; ++i) {
        VertexID id;
        LabelID label;
        ui degree;
        infile >> type >> id >> label >> degree;
        labels_[id] = label;
        labels_frequency_[label] += 1;
        neighbors_[id].reserve(degree);
    }
    for (int i = 0; i < edges_count_; ++i) {
        VertexID begin;
        VertexID end;
        infile >> type >> begin >> end;
        neighbors_[begin].push_back(end);
        neighbors_[end].push_back(begin);
    }

    auto end = std::chrono::high_resolution_clock::now();
    if (big_graph) std::cout << "read graph cost: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
    buildIndex();
}

void Graph::loadGraphFromFile(const std::string& file_path) {
    FastReader reader(file_path);
    char type;
    bool big_graph = false;
    type = reader.get_alpha();
    vertices_count_ = reader.get_unsigned();
    edges_count_ = reader.get_unsigned();
    big_graph = vertices_count_ > 5000;
    auto start = std::chrono::high_resolution_clock::now();
    neighbors_.resize(vertices_count_);
    labels_.resize(vertices_count_);
    for (ui i = 0; i < vertices_count_; ++i) {
        char v_type = reader.get_alpha();  // 'v'
        ui id = reader.get_unsigned();
        LabelID label = reader.get_unsigned();
        ui degree = reader.get_unsigned();

        labels_[id] = label;
        labels_frequency_[label] += 1;
        neighbors_[id].reserve(degree);
    }

    for (ui i = 0; i < edges_count_; ++i) {
        char e_type = reader.get_alpha();  // 'e'
        VertexID begin = reader.get_unsigned();
        VertexID end = reader.get_unsigned();

        neighbors_[begin].push_back(end);
        neighbors_[end].push_back(begin);
    }
    auto end = std::chrono::high_resolution_clock::now();
    if (big_graph) std::cout << "read graph cost: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;

    buildIndex();
}

void Graph::dumpCompressedGraph(const std::string& file_path) {
    /**
     * format:
     * |V| |E|
     * label degree neighbor1 neighbor2 ...
     * label degree neighbor1 neighbor2 ...
     * ...
     * label degree neighbor1 neighbor2 ...
     * label_pair_num
     * u_label v_label edge_num u1 v1 u2 v2 ...
     * u_label v_label edge_num u1 v1 u2 v2 ...
     * ...
     */
    std::ofstream fout(file_path);
    if (fout.is_open() == false) {
        std::cerr << "Error opening file: " << file_path << std::endl;
        return;
    }
    fout << vertices_count_ << " " << edges_count_ << "\n";
    for (ui i = 0; i < vertices_count_; ++i) {
        fout << labels_[i] << " " << neighbors_[i].size() << " ";
        for (auto nbr : neighbors_[i]) {
            fout << nbr << " ";
        }
        fout << std::endl;
    }
    fout << edge_index1_.size() << "\n";
    for (auto& item : edge_index1_) {
        uint32_t u_label = item.first >> 32;
        uint32_t v_label = item.first & 0xFFFFFFFF;
        fout << u_label << " " << v_label << " " << item.second.size() << " ";
        for (auto& edge : item.second) {
            fout << edge.vertices_[0] << " " << edge.vertices_[1] << " ";
        }
        fout << "\n";
    }
    // output nlf_
    for (int i = 0; i < vertices_count_; ++i) {
        fout << nlf_[i].size() << " ";
        for (auto& element : nlf_[i]) {
            fout << element.first << " " << element.second << " ";
        }
        fout << "\n";
    }

    fout.close();
}

void Graph::loadCompressedGraph(const std::string& file_path) {
    FastReader reader(file_path);

    vertices_count_ = reader.get_unsigned();
    edges_count_ = reader.get_unsigned();

    bool big_graph = vertices_count_ > 5000;

    auto t0 = std::chrono::high_resolution_clock::now();
    neighbors_.resize(vertices_count_);
    labels_.resize(vertices_count_);
    for (ui i = 0; i < vertices_count_; ++i) {
        LabelID label = reader.get_unsigned();
        ui degree = reader.get_unsigned();
        labels_[i] = label;
        labels_frequency_[label] += 1;
        neighbors_[i].resize(degree);
        for (ui j = 0; j < degree; ++j) {
            neighbors_[i][j] = reader.get_unsigned();
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    if (big_graph) std::cout << "read graph cost: " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms" << std::endl;

    ui label_pair_num = reader.get_unsigned();
    for (ui i = 0; i < label_pair_num; ++i) {
        uint32_t u_label = reader.get_unsigned();
        uint32_t v_label = reader.get_unsigned();
        uint64_t key = (uint64_t)u_label << 32 | v_label;
        ui edge_num = reader.get_unsigned();
        std::vector<Edge> edge_list;
        edge_list.resize(edge_num);
        for (ui j = 0; j < edge_num; ++j) {
            uint32_t u = reader.get_unsigned();
            uint32_t v = reader.get_unsigned();
            edge_list[j] = Edge(u, v);
        }
        edge_index1_[key] = edge_list;
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    if (big_graph) std::cout << "read edge index cost: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms" << std::endl;

    BuildReverseIndex();
    auto t3 = std::chrono::high_resolution_clock::now();
    if (big_graph) std::cout << "build reverse index cost: " << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count() << " ms" << std::endl;

#if OPTIMIZED_LABELED_GRAPH == 1
    if (enable_label_offset_) {
        // BuildNLF();
        nlf_.resize(vertices_count_);
        for (ui i = 0; i < vertices_count_; ++i) {
            ui nlf_size = reader.get_unsigned();
            nlf_[i].resize(nlf_size);
            for (ui j = 0; j < nlf_size; ++j) {
                uint32_t label = reader.get_unsigned();
                uint32_t freq = reader.get_unsigned();
                nlf_[i][j] = std::make_pair(label, freq);
            }
        }
        auto t4 = std::chrono::high_resolution_clock::now();
        if (big_graph) std::cout << "build NLF cost: " << std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count() << " ms" << std::endl;
    }
#endif
}

void Graph::loadGraphWithoutMeta(const std::string& file_path) {
    std::vector<std::tuple<VertexID, LabelID, ui>> v_tuples;
    std::vector<std::tuple<VertexID, VertexID>> e_tuples;

    std::ifstream infile(file_path);
    char type;
    while (infile >> type) {
        if (type == 'v') {
            VertexID id;
            LabelID label;
            infile >> id >> label;
            v_tuples.emplace_back(id, label, 0);
        } else if (type == 'e') {
            VertexID begin;
            VertexID end;
            LabelID label;
            infile >> begin >> end >> label;
            e_tuples.emplace_back(begin, end);
            std::get<2>(v_tuples[begin]) += 1;
            std::get<2>(v_tuples[end]) += 1;
        }
    }
    infile.close();

    loadGraphFromTuples(v_tuples, e_tuples);
}

void Graph::loadGraphFromTuples(
    const std::vector<std::tuple<VertexID, LabelID, ui>>& v_tuples,
    const std::vector<std::tuple<VertexID, VertexID>>& e_tuples) {
    vertices_count_ = (ui)v_tuples.size();
    edges_count_ = (ui)e_tuples.size();

    neighbors_.resize(vertices_count_);
    labels_.resize(vertices_count_);

    for (ui i = 0; i < vertices_count_; ++i) {
        VertexID id = std::get<0>(v_tuples[i]);
        LabelID label = std::get<1>(v_tuples[i]);
        ui degree = std::get<2>(v_tuples[i]);

        labels_[id] = label;

        if (labels_frequency_.find(label) == labels_frequency_.end()) {
            labels_frequency_[label] = 0;
        }

        labels_frequency_[label] += 1;
        neighbors_[id].reserve(degree);
    }

    for (ui i = 0; i < edges_count_; ++i) {
        VertexID begin = std::get<0>(e_tuples[i]);
        VertexID end = std::get<1>(e_tuples[i]);

        neighbors_[begin].push_back(end);
        neighbors_[end].push_back(begin);
    }

    for (ui i = 0; i < vertices_count_; ++i) {
        std::sort(neighbors_[i].begin(), neighbors_[i].end());
    }

    BuildReverseIndex();
    if (enable_edge_index_) {
        buildEdgeIndex();
    }

#if OPTIMIZED_LABELED_GRAPH == 1
    if (enable_label_offset_) {
        BuildNLF();
    }
#endif
}

void Graph::printGraphMetaData() {
    std::cout << "|V|: " << vertices_count_ << ", |E|: " << edges_count_
              << ", |\u03A3|: " << getMaxLabelId() << std::endl;
    std::cout << "Max Degree: " << getGraphMaxDegree()
              << ", Max Label Frequency: " << getGraphMaxLabelFrequency() << std::endl;
}

void Graph::buildCoreTable() {
    if (core_table_ != nullptr) return;
    core_table_ = new int[vertices_count_];
    GraphOperations::getKCore(this, core_table_);

    for (ui i = 0; i < vertices_count_; ++i) {
        if (core_table_[i] > 1) {
            core_length_ += 1;
        }
    }
}

void Graph::buildEdgeIndex() {
    Edge cur_edge;

    for (uint32_t u = 0; u < vertices_count_; ++u) {
        uint32_t u_l = getVertexLabel(u);

        cur_edge.vertices_[0] = u;

        for (auto v : neighbors_[u]) {
            uint32_t v_l = getVertexLabel(v);

            uint64_t key = (uint64_t)u_l << 32 | v_l;
            cur_edge.vertices_[1] = v;

            auto iter = edge_index1_.find(key);
            if (iter != edge_index1_.end()) {
                iter->second.push_back(cur_edge);
            } else {
                edge_index1_[key] = std::vector<Edge>({cur_edge});
            }
        }
    }
}

void Graph::PrintDot(std::ofstream& out) {
    out << "graph {" << std::endl;
    for (ui i = 0; i < this->getVerticesCount(); ++i) {
        out << i << " [label=\"" << i << ":" << this->getVertexLabel(i) << "\"];"
            << std::endl;
    }

    for (ui i = 0; i < this->getVerticesCount(); ++i) {
        ui count;
        const VertexID* neighbors = this->getVertexNeighbors(i, count);
        for (ui j = 0; j < count; ++j) {
            if (i < neighbors[j]) {
                out << i << " -- " << neighbors[j] << ";" << std::endl;
            }
        }
    }

    out << "}" << std::endl;
}

void Graph::Dump(std::ofstream& out) {
    out << "t " << vertices_count_ << " " << edges_count_ << std::endl;
    for (ui i = 0; i < vertices_count_; ++i) {
        out << "v " << i << " " << labels_[i] << " "
            << getVertexDegree(i) << std::endl;
    }

    for (ui i = 0; i < vertices_count_; ++i) {
        ui count;
        const VertexID* neighbors = getVertexNeighbors(i, count);
        for (ui j = 0; j < count; ++j) {
            if (i < neighbors[j]) {
                out << "e " << i << " " << neighbors[j] << std::endl;
            }
        }
    }
}

void Graph::Dump(std::string file_name) {
    std::ofstream out(file_name);
    Dump(out);
    out.close();
}

void Graph::GetTupleFormat(
    std::vector<std::tuple<VertexID, LabelID, ui>>& v_tuples,
    std::vector<std::tuple<VertexID, VertexID>>& e_tuples) {
    v_tuples.clear();
    e_tuples.clear();

    for (ui i = 0; i < vertices_count_; ++i) {
        v_tuples.emplace_back(i, labels_[i], getVertexDegree(i));
    }

    for (ui i = 0; i < vertices_count_; ++i) {
        ui count;
        const VertexID* neighbors = getVertexNeighbors(i, count);
        for (ui j = 0; j < count; ++j) {
            if (i < neighbors[j]) {
                e_tuples.emplace_back(i, neighbors[j]);
            }
        }
    }
}

Graph* Graph::Clone() {
    Graph* ret = new Graph(true);
    std::vector<std::tuple<VertexID, LabelID, ui>> v_tuples;
    std::vector<std::tuple<VertexID, VertexID>> e_tuples;
    GetTupleFormat(v_tuples, e_tuples);
    ret->loadGraphFromTuples(v_tuples, e_tuples);
    return ret;
}

void Graph::InsertEdge(VertexID u, VertexID v) {
    assert(enable_edge_index_ == false);
    edges_count_++;

    for (int i = 0; i < 2; ++i) {
        // update neighbors_
        int pos = std::lower_bound(neighbors_[u].begin(), neighbors_[u].end(), v) - neighbors_[u].begin();
        neighbors_[u].insert(neighbors_[u].begin() + pos, v);
        // update edge_index_
        auto u_l = getVertexLabel(u), v_l = getVertexLabel(v);
        auto key1 = (uint64_t)u_l << 32 | v_l;
        Edge edge1 = Edge(u, v);
        // update nlf_
#if OPTIMIZED_LABELED_GRAPH == 1
        { 
            int pos = std::lower_bound(nlf_[u].begin(), nlf_[u].end(), std::make_pair(v_l, 0),
                                       [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) {
                                           return a.first < b.first;
                                       }) -
                      nlf_[u].begin();
            if (pos < nlf_[u].size() && nlf_[u][pos].first == v_l) {
                nlf_[u][pos].second += 1;
            } else {
                nlf_[u].insert(nlf_[u].begin() + pos, std::make_pair(v_l, 1));
            }
        }
#endif
        std::swap(u, v);
    }
}

void Graph::DeleteEdge(VertexID u, VertexID v) {
    assert(enable_edge_index_ == false);

    edges_count_--;
    for (int i = 0; i < 2; ++i) {
        // update neighbors_
        int pos = std::lower_bound(neighbors_[u].begin(), neighbors_[u].end(), v) - neighbors_[u].begin();
        assert(pos < neighbors_[u].size() && neighbors_[u][pos] == v);
        neighbors_[u].erase(neighbors_[u].begin() + pos);
        // update edge_index_
        auto u_l = getVertexLabel(u), v_l = getVertexLabel(v);
        auto key1 = (uint64_t)u_l << 32 | v_l;
        Edge edge1 = Edge(u, v);

        // update nlf_
#if OPTIMIZED_LABELED_GRAPH == 1
        {
            int pos = std::lower_bound(nlf_[u].begin(), nlf_[u].end(), std::make_pair(v_l, 0),
                                       [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) {
                                           return a.first < b.first;
                                       }) -
                      nlf_[u].begin();
            assert(pos < nlf_[u].size() && nlf_[u][pos].first == v_l);
            nlf_[u][pos].second -= 1;
            if (nlf_[u][pos].second == 0) {
                nlf_[u].erase(nlf_[u].begin() + pos);
            }
        }

#endif
        std::swap(u, v);
    }
}