#include "mat_view.h"

#include "pretty_print.h"



void MatView::Dump(std::string file_name) {
    std::ofstream fout(file_name);
    sqg_.Dump(fout);
    int sdn_num = sdns_.size();
    for (int i = 0; i < sdn_num; ++i) {
        sdns_[i].Dump(fout);
    }
    DumpSuperEdge(fout);
    cs_.Dump(fout);
    fout.close();
}

static void DumpGroup(std::ofstream& fout, SuperDataEdge::Index& index) {
    int group_n = index.group_id2embedding_ids_.size();
    fout << group_n << std::endl;
    for (int i = 0; i < group_n; ++i) {
        auto group_size = index.group_id2embedding_ids_[i].size();
        fout << group_size << " ";
        for (auto id : index.group_id2embedding_ids_[i]) {
            fout << id << " ";
        }
        fout << std::endl;
    }
}

static void DumpLink(std::ofstream& fout, std::vector<std::vector<int>>& link,
                     std::vector<bool>& materialized) {
    int group_n = link.size();
    for (int i = 0; i < group_n; ++i) {
        auto is_mat = materialized.empty() || materialized[i];
        fout << is_mat << " ";
        if (is_mat) {
            auto link_size = link[i].size();
            fout << link_size << " ";
            for (auto id : link[i]) {
                fout << id << " ";
            }
        }
        fout << std::endl;
    }
}

void SuperDataEdge::Dump(std::ofstream& fout) {
    fout << su_ << " " << sv_ << std::endl;
    DumpGroup(fout, su_index_);
    DumpGroup(fout, sv_index_);
    DumpLink(fout, linked_groups_[0], materialized_[0]);
    DumpLink(fout, linked_groups_[1], materialized_[1]);
}

void MatView::DumpSuperEdge(std::ofstream& fout) {
    int sdn_n = sdns_.size();
    int super_edge_n = 0;
    for (int i = 0; i < sdn_n; ++i) {
        auto u = i;
        for (int j = 0; j < sqg_.adj_[u].size(); ++j) {
            auto v = sqg_.adj_[u][j].dst;
            if (u >= v) continue;
            super_edge_n++;
        }
    }
    fout << super_edge_n << std::endl;
    for (int i = 0; i < sdn_n; ++i) {
        auto u = i;
        for (int j = 0; j < sqg_.adj_[u].size(); ++j) {
            auto v = sqg_.adj_[u][j].dst;
            if (u >= v) continue;
            ses_[u][v].Dump(fout);
        }
    }
}

void MatView::LoadSQG() {
    std::ifstream infile(file_name_);
    sqg_.Load(infile);
    infile.close();
}

void MatView::Load() {
    std::ifstream infile(file_name_);
    int sdn_num;
    sqg_.Load(infile);
    sdns_.resize(sqg_.sqns_.size());
    sdn_num = sdns_.size();
    for (int i = 0; i < sdn_num; ++i) {
        sdns_[i].Load(infile);
    }
    LoadSuperEdge(infile);
    cs_.Load(infile);
    BuildTries(false);
}


static void LoadIndex(std::ifstream& fin, SuperDataEdge::Index& index) {
    int group_n;
    fin >> group_n;
    index.group_id2embedding_ids_.resize(group_n);
    int embedding_cnt = 0;
    for (int i = 0; i < group_n; ++i) {
        int group_size;
        fin >> group_size;
        index.group_id2embedding_ids_[i].resize(group_size);
        for (int j = 0; j < group_size; ++j) {
            fin >> index.group_id2embedding_ids_[i][j];
        }
        embedding_cnt += group_size;
    }
    index.embedding_id2group_id_.resize(embedding_cnt);
    for (int i = 0; i < group_n; ++i) {
        for (auto id : index.group_id2embedding_ids_[i]) {
            index.embedding_id2group_id_[id] = i;
        }
    }
}

static void LoadLink(std::ifstream& fin, int group_n, std::vector<std::vector<int>>& link,
                     std::vector<bool>& materialized, bool& all_materialized) {
    materialized.resize(group_n);
    link.resize(group_n);
    all_materialized = true;
    for (int i = 0; i < group_n; ++i) {
        int is_mat;
        fin >> is_mat;
        materialized[i] = is_mat;
        if (is_mat) {
            int link_size;
            fin >> link_size;
            link[i].resize(link_size);
            for (int j = 0; j < link_size; ++j) {
                fin >> link[i][j];
            }
        } else {
            all_materialized = false;
        }
    }
}

void SuperDataEdge::Load(std::ifstream& fin, int su, int sv) {
    su_ = su;
    sv_ = sv;
    LoadIndex(fin, su_index_);
    LoadIndex(fin, sv_index_);
    LoadLink(fin, su_index_.group_id2embedding_ids_.size(), linked_groups_[0], materialized_[0], all_materialized_[0]);
    LoadLink(fin, sv_index_.group_id2embedding_ids_.size(), linked_groups_[1], materialized_[1], all_materialized_[1]);
}

void MatView::LoadSuperEdge(std::ifstream& fin) {
    ses_.resize(sqg_.sqns_.size());
    for (int i = 0; i < sqg_.sqns_.size(); ++i) {
        ses_[i].resize(sqg_.sqns_.size());
    }
    int super_edge_n;
    fin >> super_edge_n;
    for (int i = 0; i < super_edge_n; ++i) {
        int u, v;
        fin >> u >> v;
        ses_[u][v].Load(fin, u, v);
    }
}


void MatView::Release() {
    // todo
}

unsigned HashEmbedding(unsigned* embedding, int embedding_width) {
    unsigned res = 0;
    for (int i = 0; i < embedding_width; ++i) {
        res = (res + 1) * 131 + embedding[i];
    }
    return res;
}

void MatView::PrintSE(int su, int sv) {
    std::cout << "su = " << su << ", sv = " << sv << "\n";
    auto& se = ses_[su][sv];
    auto su_g_num = se.su_index_.group_id2embedding_ids_.size();
    auto sv_g_num = se.sv_index_.group_id2embedding_ids_.size();
    auto su_embedding_len = sdns_[su].embedding_width_;
    auto sv_embedding_len = sdns_[sv].embedding_width_;
    for (int i = 0; i < su_g_num; ++i) {
        auto su_g_id = i;
        for (auto item : se.linked_groups_[0][su_g_id]) {
            auto sv_g_id = item;
            std::cout << "  su_g: " << su_g_id << ", " << "sv_g: " << sv_g_id << "\n";
            for (auto su_e_id : se.su_index_.group_id2embedding_ids_[su_g_id]) {
                for (auto sv_e_id : se.sv_index_.group_id2embedding_ids_[sv_g_id]) {
                    auto su_embedding = sdns_[su].GetEmbedding(su_e_id);
                    auto sv_embedding = sdns_[sv].GetEmbedding(sv_e_id);
                    std::cout << "    " << su_e_id << ": "
                              << std::vector<int>(su_embedding,
                                                  su_embedding + su_embedding_len)
                              << "--" << sv_e_id << ": "
                              << std::vector<int>(sv_embedding,
                                                  sv_embedding + sv_embedding_len)
                              << "\n";
                }
            }
        }
    }
}

void MatView::Print() {
    auto sdn_num = sdns_.size();
    for (int i = 0; i < sdn_num; ++i) {
        auto su = i;
        for (auto item : sqg_.adj_[i]) {
            auto sv = item.dst;
            if (su > sv) continue;
            PrintSE(su, sv);
        }
    }
}

unsigned long long MatView::MemoryCost() {
    unsigned long long ret = 0;
    int n = sqg_.sqns_.size();

    // collect sdns memory cost
    for (auto& sdn : sdns_) {
        ret += sdn.MemoryCost();
    }

    // collect ses memory cost
    for (ui u = 0; u < n; ++u) {
        for (auto item : sqg_.adj_[u]) {
            auto v = item.dst;
            if (u < v) {
                ret += ses_[u][v].MemoryCost();
            }
        }
    }

    // cs_ memory cost
    ret += cs_.MemoryCost();

    return ret;
}

SuperDataEdgeView GetSuperDataEdgeView(int su, int sv, std::vector<std::vector<SuperDataEdge>>& ses) {
    return su < sv ? SuperDataEdgeView(ses[su][sv], 0) : SuperDataEdgeView(ses[sv][su], 1);
}