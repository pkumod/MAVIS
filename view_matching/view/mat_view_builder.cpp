#include "mat_view_builder.h"

#include <future>

#include "edge_materializer.h"
#include "global_vars.h"
#include "materializer.h"
#include "node_materializer.h"
#include "pretty_print.h"
#include "simple_tree_partition.h"
#include "subgraph_filter.h"
#include "tree_partitioner.h"

void MergeSuperNode(std::vector<std::vector<ui>>& result, Graph* view_pattern, SubgraphEstimator* estimator) {
    if (view_pattern->core_table_ == nullptr) {
        view_pattern->buildCoreTable();
    }

    auto CheckSuperEdge = [&](ui s1, ui s2) {
        for (auto u : result[s1]) {
            for (auto v : result[s2]) {
                if (view_pattern->checkEdgeExistence(u, v)) {
                    return true;
                }
            }
        }
        return false;
    };

    for (;;) {
        std::vector<ui> degree(result.size(), 0);
        // for (int s = 0; s < result.size(); ++s) {
        //     for (int s1 = 0; s1 < result.size(); ++s1) {
        //         if (s != s1 && CheckSuperEdge(s, s1)) {
        //             degree[s]++;
        //         }
        //     }
        // }

        long double min_eval = 1e30;
        int min_s1 = -1, min_s2 = -1;
        for (int s1 = 0; s1 < result.size(); ++s1) {
            // if (degree[s1] < 2) continue;
            if (view_pattern->core_table_[result[s1][0]] < 2) continue;
            for (int s2 = s1 + 1; s2 < result.size(); ++s2) {
                // if (degree[s2] < 2) continue;
                if (view_pattern->core_table_[result[s2][0]] < 2) continue;

                if (CheckSuperEdge(s1, s2) == false) continue;{
                    auto s3 = result[s1];
                    s3.insert(s3.end(), result[s2].begin(), result[s2].end());
                    auto s3_eval = estimator->Estimate(s3);
                    if (s3_eval < min_eval) {
                        min_eval = s3_eval;
                        min_s1 = s1;
                        min_s2 = s2;
                    }
                }
                
            }
        }
        if (min_eval < MAX_EMBEDDING_NUM) {
            auto s1 = result[min_s1];
            auto s2 = result[min_s2];
            s1.insert(s1.end(), s2.begin(), s2.end());
            result.erase(result.begin() + min_s2);
            result[min_s1] = s1;
        } else {
            break;
        }
    }
}

bool MatViewBuilder::Build(Graph* view_pattern, Graph* data_graph, MatView& mv, bool naive_partition) {
    auto start = std::chrono::high_resolution_clock::now();
    // filter
    bool have_answer;
#ifdef HOMOMORPHIC
    SubgraphFilter filter(view_pattern, data_graph, false /* isomorphic */);
#else
    SubgraphFilter filter(view_pattern, data_graph);
#endif
    CandidateSpace cs;
    bool not_empty = filter.Build(cs);
    if (!not_empty) return false;
    std::cout << "finished Build::Filter\n";

    // estimator
    std::unique_ptr<SubgraphEstimator> estimator(new SubgraphEstimator(view_pattern, data_graph, cs));

    // build super query graph
    std::unique_ptr<AbstractPartitioner> query_decomposer;
    if (naive_partition) {
        query_decomposer.reset(new SimpleTreePartition(estimator.get(), view_pattern));
    } else {
        query_decomposer.reset(new TreePartitioner(estimator.get(), view_pattern, true));
    }
    std::vector<std::vector<ui>> result;
    auto start1 = std::chrono::high_resolution_clock::now();
    query_decomposer->QueryDecomposition(result);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto query_decomposition_cost = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count();
    std::cout << "decomposition cost: " << MICROSECOND2SECOND(query_decomposition_cost) << "s" << std::endl;

    if (g_merge_super_node) {
        MergeSuperNode(result, view_pattern, estimator.get());
    }

    std::cout << "partition view result: " << result << std::endl;

    TreePartitionGraph sqg(result, view_pattern->Clone());
#ifdef ENABLE_DOT_OUTPUT
    EnsureDotDir();
    sqg.PrintDot("dot_file/super_query_graph.dot");
#endif
    std::cout << "finished Build::BuildSuperQueryGraph\n";

    have_answer = Materialize(view_pattern, data_graph, estimator.get(), sqg, cs, mv);
    std::cout << "finished Build::MatView\n";

    if (have_answer) {
        std::cout << "memory cost(bytes): " << mv.MemoryCost() << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    g_build_view_cost = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return have_answer;
}

bool MatViewBuilder::Materialize(Graph* view_pattern, Graph* data_graph, SubgraphEstimator* estimator,
                                 TreePartitionGraph& sqg, CandidateSpace& cs, MatView& mv, bool incremental) {
    Materializer imv(view_pattern, data_graph, estimator, sqg, cs);

    bool have_answer = imv.Exec();
    mv.sqg_ = std::move(imv.sqg_);
    if (have_answer) {
        mv.sdns_ = std::move(imv.sdns_);
        mv.ses_ = std::move(imv.ses_);
        mv.cs_ = std::move(imv.cs_);
        
        // auto query_graph = mv.sqg_.query_graph_.get();
        // mv.cand_bit_sets_.resize(query_graph->getVerticesCount(), CompressedBitSet(data_graph->getVerticesCount()));
        // for (ui v = 0; v < query_graph->getVerticesCount(); ++v) {
        //     for (auto u : imv.cs_.vcand_[v]) {
        //         mv.cand_bit_sets_[v].Set(u);
        //     }
        // }

        mv.BuildTries(incremental);
    }
    else {
        oversized_ = imv.oversized_;
    }

    return have_answer;
}

std::string MatViewBuilder::BuildWithinTimeLimit(Graph* view_pattern, Graph* data_graph, MatView& mv, unsigned time_limit) {
    g_exit = false;
    std::future<bool> future = std::async(
        std::launch::async, [this, view_pattern, data_graph, &mv]() {
            return this->Build(view_pattern, data_graph, mv, g_use_naive_partition);
        });

    std::future_status status;
    do {
        status = future.wait_for(std::chrono::seconds(time_limit));
        if (status == std::future_status::deferred) {
            std::cout << "Deferred\n";
            exit(-1);
        } else if (status == std::future_status::timeout) {
            g_exit = true;
        }
    } while (status != std::future_status::ready);
    if (g_exit == true) return "timeout";
    if (future.get() == 0) return "no answer";
    return "have answer";
}
