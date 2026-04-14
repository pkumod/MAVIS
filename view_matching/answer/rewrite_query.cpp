#include "rewrite_query.h"

#include <queue>

#include "BitSet.h"
#include "encoder.h"
#include "enumerate.h"
#include "global_vars.h"
#include "pretty_print.h"
#include "relation/catalog.h"
#include "simple_matching.h"

void RewriteQuery::PrintDot(std::ostream& out, Graph* query_graph) {
    bool label_mode = true;
    out << "graph G{\n";
    for (int i = 0; i < nodes_.size(); ++i) {
        auto rw_node = nodes_[i];
        if (rw_node->type_ == Type::MATERIALIZED) {
            out << "subgraph cluster_" << i << " {\n";
            out << "style=filled;\n";
            out << "label=\"r" << i << "s" << rw_node->sqn_->id_ << "\";\n";
            std::vector<unsigned>& sv_list = rw_node->sqn_->node_list_;
            // output node within the super node
            for (auto sv : sv_list) {
                out << "  n" << rw_node->sv2qv_[sv] << "_" << sv;
                if (label_mode) {
                    out << " [label=\"n" << rw_node->sv2qv_[sv] << "_" << sv << ":"
                        << query_graph->getVertexLabel(rw_node->sv2qv_[sv]) << "\"]";
                }
                out << ";\n";
            }
            // output edge within the super node
            for (auto sv : sv_list) {
                for (auto dst : rw_node->sqn_->link_info_[sv]) {
                    if (sv > dst) continue;
                    out << "  n" << rw_node->sv2qv_[sv] << "_" << sv << " -- n" << rw_node->sv2qv_[dst] << "_" << dst << ";\n";
                }
            }
            for (auto edge : rw_node->patch_edges_) {
                out << "  n" << rw_node->sv2qv_[edge.first] << "_" << edge.first
                    << " -- n" << rw_node->sv2qv_[edge.second] << "_"
                    << edge.second << " [style=dotted color=red];\n";
            }
            out << "}\n";
        } else {
            out << "subgraph cluster_" << i << " {\n";
            out << "label=\"r" << i << "\";\n";
            out << "style=filled;\n";
            out << "color=lightgrey;\n";
            out << "node [style=filled,color=white];\n";
            out << "  n" << rw_node->node_id_;
            if (label_mode) {
                out << " [label=\"n" << rw_node->node_id_ << ":" << query_graph->getVertexLabel(rw_node->node_id_) << "\"]";
            }
            out << ";\n";
            out << "}\n";
        }
    }
    for (int i = 0; i < link_.size(); ++i) {
        for (auto edge : link_[i]) {
            if (i > edge->dst_) continue;
            if (edge->type_ == Type::NORMAL) {
                auto normal_edge = dynamic_cast<NormalEdge*>(edge);
                auto src_node = nodes_[normal_edge->src_];
                auto dst_node = nodes_[normal_edge->dst_];
                for (auto src : src_node->GetNodeList()) {
                    for (auto dst : dst_node->GetNodeList()) {
                        if (!query_graph->checkEdgeExistence(src, dst)) continue;
                        std::string src_str, dst_str;
                        if (src_node->type_ == Type::MATERIALIZED) {
                            src_str = "n" + std::to_string(src) + "_" + std::to_string(src_node->qv2sv_[src]);
                        } else {
                            src_str = "n" + std::to_string(src);
                        }
                        if (dst_node->type_ == Type::MATERIALIZED) {
                            dst_str = "n" + std::to_string(dst) + "_" + std::to_string(dst_node->qv2sv_[dst]);
                        } else {
                            dst_str = "n" + std::to_string(dst);
                        }
                        out << "  " << src_str << " -- " << dst_str << " [style=dashed];\n";
                    }
                }
            } else {
                auto mat_edge = dynamic_cast<MatEdge*>(edge);
                auto src_node = nodes_[mat_edge->src_];
                auto dst_node = nodes_[mat_edge->dst_];
                for (auto inner_edge : mat_edge->super_edge_info_->src_dst_pairs) {
                    auto src = src_node->sv2qv_[inner_edge.first];
                    auto dst = dst_node->sv2qv_[inner_edge.second];
                    std::pair<ui, ui> edge = std::make_pair(src, dst);
                    out << "  n" << src << "_" << inner_edge.first << " -- n" << dst << "_" << inner_edge.second << ";\n";
                }
                for (auto [u_pos, v_pos] : mat_edge->patch_edges_) {
                    auto qu = src_node->GetInnerOrder()[u_pos];
                    auto qv = dst_node->GetInnerOrder()[v_pos];
                    auto src = src_node->qv2sv_[qu];
                    auto dst = dst_node->qv2sv_[qv];
                    out << "  n" << qu << "_" << src << " -- n" << qv << "_" << dst << " [style=dotted color=red];\n";
                }
            }
        }
    }
    out << "}\n";
}

bool CheckEdgeExistence(Graph* query_graph, RewriteQuery& rq, int u, int v) {
    auto rw_u = rq.nodes_[u];
    auto rw_v = rq.nodes_[v];
    auto v_list1 = rw_u->GetNodeList();
    auto v_list2 = rw_v->GetNodeList();
    for (auto v1 : v_list1) {
        for (auto v2 : v_list2) {
            if (query_graph->checkEdgeExistence(v1, v2)) {
                return true;
            }
        }
    }
    return false;
}

bool LinkMatEdge(RewriteQuery& rq, MatView& mv, Graph* query_graph, int u, int v) {
    auto rw_u = rq.nodes_[u];
    auto rw_v = rq.nodes_[v];
    const auto& v_list1 = rw_u->GetNodeList();
    const auto& v_list2 = rw_v->GetNodeList();
    bool linked = false;
    auto view_pattern = mv.sqg_.query_graph_.get();
    if (rw_u->type_ == RewriteQuery::Type::MATERIALIZED && rw_v->type_ == RewriteQuery::Type::MATERIALIZED) {
        TreePartitionGraph::SuperEdgeInfo& sde = mv.sqg_.adj_mat_[rw_u->sqn_->id_][rw_v->sqn_->id_];
        if (sde.src_dst_pairs.empty() == false) {
            int covered_edge_num = 0;
            std::vector<std::pair<ui, ui>> patch_edges;
            for (int qpos1 = 0; qpos1 < v_list1.size(); ++qpos1) {
                for (int qpos2 = 0; qpos2 < v_list2.size(); ++qpos2) {
                    auto qv1 = v_list1[qpos1];
                    auto qv2 = v_list2[qpos2];
                    if (query_graph->checkEdgeExistence(qv1, qv2) == false) continue;
                    auto sv1 = rw_u->qv2sv_[qv1];
                    auto sv2 = rw_v->qv2sv_[qv2];

                    if (view_pattern->checkEdgeExistence(sv1, sv2)) {
                        covered_edge_num++;
                    } else {
                        patch_edges.push_back(std::make_pair(qpos1, qpos2));
                    }
                }
            }
            std::vector<ui> src_port_vertices, dst_port_vertices;
            for (auto pu : sde.src_set) src_port_vertices.push_back(rw_u->sv2qv_[pu]);
            for (auto pv : sde.dst_set) dst_port_vertices.push_back(rw_v->sv2qv_[pv]);

            auto d = rw_u->sqn_->id_ > rw_v->sqn_->id_;
            rq.link_[u].push_back(new RewriteQuery::MatEdge(u, v,
                                                            &sde,
                                                            d ? &mv.ses_[rw_v->sqn_->id_][rw_u->sqn_->id_] : &mv.ses_[rw_u->sqn_->id_][rw_v->sqn_->id_],
                                                            d,
                                                            patch_edges,
                                                            covered_edge_num,
                                                            src_port_vertices,
                                                            dst_port_vertices));
            linked = true;
        }
    }
    return linked;
}

void RewriteQueryFromMatView(Graph* query_graph, MatView& mv, std::vector<int>& vv2qv, RewriteQuery& rq, MyBitSet* covered) {
    std::vector<int> qv2vv(query_graph->getVerticesCount(), -1);
    for (int i = 0; i < vv2qv.size(); ++i) {
        qv2vv[vv2qv[i]] = i;
    }
    std::vector<bool> used(mv.sqg_.sqns_.size(), false);
    int rw_n_st = rq.nodes_.size();
    // check no overlap
    for (int i = 0; i < mv.sqg_.sqns_.size(); ++i) {
        auto& sqn = mv.sqg_.sqns_[i];
        for (auto u : sqn.node_list_) {
            if (covered->Test(vv2qv[u])) return;
        }
    }
    // load mat view
    auto start = std::chrono::high_resolution_clock::now();
    if (mv.loaded_ == false) {
        std::cout << "load mat view " << mv.file_name_ << std::endl;
        mv.Load();
        mv.loaded_ = true;
#ifdef ENABLE_DOT_OUTPUT
        mv.sqg_.PrintDot("dot_file/view.dot");
#endif
        for (int i = 0; i < mv.sdns_.size(); ++i) {
            std::cout << "SuperDataNode " << i << " embedding num: " << mv.sdns_[i].GetEmbeddingNum() << std::endl;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    g_load_cost += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    // set cover
    for (int i = 0; i < mv.sqg_.sqns_.size(); ++i) {
        auto& sqn = mv.sqg_.sqns_[i];
        bool valid = true;
        if (sqn.node_list_.size() == 1) {  // no need cover non-core node
            auto u_in_sqn = sqn.node_list_[0];
            auto u_in_query = vv2qv[u_in_sqn];
            if (query_graph->getCoreValue(u_in_query) == 1) {
                valid = false;
            }
        }
        // std::cout << "valid: " << valid << std::endl;
        if (valid) {
            used[i] = true;
            for (auto vv : sqn.node_list_) {
                auto qv = vv2qv[vv];
                covered->Set(qv);
                // test if need nlf
                bool need_nlf = false;
                ui len = 0;
                auto nbrs = query_graph->getVertexNeighbors(qv, len);
                for (int j = 0; j < len; ++j) {
                    auto qv1 = nbrs[j];
                    if (qv2vv[qv1] == -1) {
                        need_nlf = true;
                        break;
                    }
                }
                rq.need_nlf_[qv] = need_nlf;
            }
        }
    }

    //  build materialized nodes
    for (int i = 0; i < mv.sqg_.sqns_.size(); ++i) {
        if (used[i] == false) continue;
        RewriteQuery::Node* mat_node = new RewriteQuery::Node();
        mat_node->sdn_ = &mv.sdns_[i];
        mat_node->sqn_ = &mv.sqg_.sqns_[i];
        for (auto u : mv.sqg_.sqns_[i].order_) {
            auto qu = vv2qv[u];
            mat_node->sv2qv_[u] = qu;
            mat_node->qv2sv_[qu] = u;
            mat_node->node_list_.push_back(qu);
        }

        // calculate patch edges
        std::set<std::pair<int, int>> edge_in_sqn;  // query edges covered by sqn
        for (auto u : mv.sqg_.sqns_[i].node_list_) {
            for (auto& v : mv.sqg_.sqns_[i].link_info_[u]) {
                if (u > v) continue;
                auto qu = mat_node->sv2qv_[u];
                auto qv = mat_node->sv2qv_[v];
                if (qu > qv) std::swap(qu, qv);
                edge_in_sqn.insert(std::make_pair(qu, qv));
            }
        }
        for (int i = 0; i < mat_node->sqn_->node_list_.size(); ++i) {
            for (int j = i + 1; j < mat_node->sqn_->node_list_.size(); ++j) {
                auto u = mat_node->sqn_->node_list_[i];
                auto v = mat_node->sqn_->node_list_[j];
                auto qu = mat_node->sv2qv_[u];
                auto qv = mat_node->sv2qv_[v];
                if (query_graph->checkEdgeExistence(qu, qv) && edge_in_sqn.count(std::make_pair(qu, qv)) == 0 && edge_in_sqn.count(std::make_pair(qv, qu)) == 0) {
                    mat_node->patch_edges_.push_back(std::make_pair(u, v));
                    mat_node->patch_edges1_.push_back(std::make_pair(mat_node->sqn_->pos_[u], mat_node->sqn_->pos_[v]));
                }
            }
        }
        rq.nodes_.push_back(mat_node);
    }
    // link materialized edges
    rq.link_.resize(rq.nodes_.size());
    for (int i = rw_n_st; i < rq.nodes_.size(); ++i) {
        for (int j = i + 1; j < rq.nodes_.size(); ++j) {
            if (i != j && CheckEdgeExistence(query_graph, rq, i, j)) {
                LinkMatEdge(rq, mv, query_graph, i, j);
                LinkMatEdge(rq, mv, query_graph, j, i);
            }
        }
    }
    // copy vertex-based view
    for (int i = 0; i < mv.sqg_.query_graph_->getVerticesCount(); ++i) {
        for (int j = i + 1; j < mv.sqg_.query_graph_->getVerticesCount(); ++j) {
            if (mv.sqg_.query_graph_->checkEdgeExistence(i, j)) {
                auto u = vv2qv[i];
                auto v = vv2qv[j];
                mv.cs_.GetCandidateEdges(i, j, rq.filtered_edge_lists_[u][v]);
            }
        }
    }
}

void OutputRQInfo(Graph* query_graph, RewriteQuery& rq) {
#ifdef ENABLE_DOT_OUTPUT
    EnsureDotDir();
    // output query.dot
    std::ofstream fout1("dot_file/query_graph.dot");
    query_graph->PrintDot(fout1);
    fout1.close();
    // draw the rewrite query
    std::ofstream fout("dot_file/rewrite_query.dot");
    rq.PrintDot(fout, query_graph);
    fout.close();
    // draw the rewrite query sketch
    fout.open("dot_file/rewrite_query_sketch.dot");
    rq.rw_graph_->PrintDot(fout);
    fout.close();
#endif
}

void CompleteRewriteQuery(Graph* query_graph, RewriteQuery& rq, MyBitSet* covered) {
    // build normal nodes
    for (int i = 0; i < query_graph->getVerticesCount(); ++i) {
        if (covered->Test(i) == false) {
            rq.nodes_.push_back(new RewriteQuery::Node(i));
        }
    }
    // link normal edges
    rq.link_.resize(rq.nodes_.size());
    for (int i = 0; i < rq.nodes_.size(); ++i) {
        for (int j = i + 1; j < rq.nodes_.size(); ++j) {
            if (CheckEdgeExistence(query_graph, rq, i, j)) {
                bool have_edge = false;
                for (auto edge : rq.link_[i]) {
                    if (edge->dst_ == j) {
                        have_edge = true;
                        break;
                    }
                }
                if (have_edge == false) {
                    rq.link_[i].push_back(new RewriteQuery::NormalEdge(i, j));
                    rq.link_[j].push_back(new RewriteQuery::NormalEdge(j, i));
                }
            }
        }
    }
    // build rw_graph
    rq.rw_graph_.reset(new Graph(true));
    std::vector<std::tuple<VertexID, LabelID, ui>> v_tuples;
    std::vector<std::tuple<VertexID, VertexID>> e_tuples;
    for (int i = 0; i < rq.nodes_.size(); ++i) {
        v_tuples.push_back(std::make_tuple(i, 0, rq.link_[i].size()));
        for (auto& edge_info : rq.link_[i]) {
            if (i < edge_info->dst_) {
                e_tuples.push_back(std::make_tuple(i, edge_info->dst_));
            }
        }
    }
    rq.rw_graph_->loadGraphFromTuples(v_tuples, e_tuples);
    // build rw_node_no_
    rq.rw_node_no_.resize(query_graph->getVerticesCount());
    for (int i = 0; i < rq.nodes_.size(); ++i) {
        const auto& nodes = rq.nodes_[i]->GetNodeList();
        for (auto node : nodes) {
            rq.rw_node_no_[node] = i;
        }
    }
    // build core table for rw_graph
    rq.rw_graph_->buildCoreTable();
}

auto RewriteQueryFromMatViewSet(Graph* query_graph, std::vector<MatView>& mv_set) {
    RewriteQuery rq;
    rq.filtered_edge_lists_.resize(query_graph->getVerticesCount());
    for (int i = 0; i < query_graph->getVerticesCount(); ++i) {
        rq.filtered_edge_lists_[i].resize(query_graph->getVerticesCount());
    }
    rq.need_nlf_.resize(query_graph->getVerticesCount(), true);

    auto start = std::chrono::high_resolution_clock::now();
    query_graph->buildCoreTable();
    MyBitSet* covered = new MyBitSet(query_graph->getVerticesCount());

    for (int i = 0; i < mv_set.size(); ++i)
    // int i = 0;
    {
        std::cout << "do matching view " << i << std::endl;
        auto& mv = mv_set[i];
        SimpleMatcher sm(query_graph, mv.sqg_.query_graph_.get());
        std::vector<std::vector<int>> embeddings;
        sm.Match(embeddings);
        for (auto mapping : embeddings) {
            RewriteQueryFromMatView(query_graph, mv, mapping, rq, covered);
        }
        // std::cout << "view " << i << " matched embedding num: " << embeddings.size() << std::endl;
        // if (embeddings.size()) {
        //     auto mapping = embeddings[0];
        //     std::cout << mapping << std::endl;
        //     auto view_pattern = mv.sqg_.query_graph_.get();
        //     for (int u = 0; u < view_pattern->getVerticesCount(); ++u) {
        //         for (int u1 = u + 1; u1 < view_pattern->getVerticesCount(); ++u1) {
        //             if (view_pattern->checkEdgeExistence(u, u1)) {
        //                 if (query_graph->checkEdgeExistence(mapping[u], mapping[u1]) == false) {
        //                     std::cout << "error edge: " << mapping[u] << "-" << mapping[u1] << std::endl;
        //                     exit(1);
        //                 }
        //             }
        //         }
        //     }
        // }
    }

    CompleteRewriteQuery(query_graph, rq, covered);
    auto end = std::chrono::high_resolution_clock::now();
    g_rewrite_cost = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "finished rewrite query, cost (ms): " << g_rewrite_cost / 1000. << std::endl;
    std::cout << "load cost (ms): " << g_load_cost / 1000. << std::endl;
    std::cout << "exclude load cost (ms): " << (g_rewrite_cost - g_load_cost) / 1000. << std::endl;
    // exit(0);

    OutputRQInfo(query_graph, rq);
    std::cout << "need nlf: " << rq.need_nlf_ << std::endl;
    return rq;
}

void RewriteQueryFromMatView(Graph* query_graph, MatView& mv, std::vector<int>& vnode2qnode, RewriteQuery& rq) {
    auto start = std::chrono::high_resolution_clock::now();
    MyBitSet* covered = new MyBitSet(query_graph->getVerticesCount());
    query_graph->buildCoreTable();
    RewriteQueryFromMatView(query_graph, mv, vnode2qnode, rq, covered);
    CompleteRewriteQuery(query_graph, rq, covered);
    auto end = std::chrono::high_resolution_clock::now();
    g_rewrite_cost = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    OutputRQInfo(query_graph, rq);
}

ProcessQueryResult ProcessQuery(Graph* data_graph, Graph* query_graph, std::vector<MatView>& mv_set, long long enum_limit, int enum_time_limit) {
    g_rewrite_cost = 0;
    g_load_cost = 0;
    g_enumerate_cost = 0;
    g_preprocess_cost = 0;
    g_encode_cost = 0;

    auto rq = RewriteQueryFromMatViewSet(query_graph, mv_set);
    g_rewrite_cost -= g_load_cost;
    std::cout << "finished rewrite query" << std::endl;

    unsigned* embeddings = nullptr;
    Enumerator enumerator(std::move(rq));
    auto embedding_num = enumerator.ExecuteWithinTimeLimit(query_graph, data_graph, enum_limit, enum_time_limit, embeddings);

    ProcessQueryResult result;
    result.embedding_num = embedding_num;
    result.enum_cost = g_enumerate_cost;
    result.encode_cost = g_encode_cost;
    result.preprocess_cost = g_preprocess_cost;
    result.rewrite_cost = g_rewrite_cost;
    result.time_out = g_exit;
    std::cout << "enum cost (ms): " << g_enumerate_cost / 1000. << std::endl;
    std::cout << "rewrite cost (ms): " << g_rewrite_cost / 1000. << std::endl;
    std::cout << "load cost (ms): " << g_load_cost / 1000. << std::endl;
    std::cout << "preprocess cost (ms): " << g_preprocess_cost / 1000. << std::endl;
    std::cout << "encode cost (ms): " << g_encode_cost / 1000. << std::endl;
    std::cout << "Time out: " << g_exit << std::endl;
    return result;
}