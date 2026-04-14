#include "MyUtils.h"

#include <algorithm>

unsigned MyHash(unsigned x) {
    x = (x + 0x7ed55d16) + (x << 12);
    x = (x ^ 0xc761c23c) ^ (x >> 19);
    x = (x + 0x165667b1) + (x << 5);
    x = (x + 0xd3a2646c) ^ (x << 9);
    x = (x + 0xfd7046c5) + (x << 3);
    x = (x ^ 0xb55a4f09) ^ (x >> 16);
    return x;
}

void MultilistMerge(const std::vector<unsigned*>& lists, const std::vector<unsigned>& sizes, std::vector<unsigned>& result) {
    std::vector<unsigned> idx(lists.size(), 0);
    std::priority_queue<PUU, std::vector<PUU>, std::greater<PUU>> pq;
    unsigned total_size = 0;
    for (unsigned i = 0; i < lists.size(); ++i) {
        if (idx[i] < sizes[i]) {
            pq.push({lists[i][idx[i]], i});
            total_size += sizes[i];
        }
    }
    result.reserve(total_size);
    while (!pq.empty()) {
        PUU top = pq.top();
        pq.pop();
        if (result.empty() || result.back() != top.first) {
            result.push_back(top.first);
        }
        if (idx[top.second] + 1 < sizes[top.second]) {
            pq.push({lists[top.second][++idx[top.second]], top.second});
        }
    }
}

void MultilistMerge(const std::vector<const unsigned*>& lists, const std::vector<unsigned>& sizes, std::vector<unsigned>& result) {
    std::vector<unsigned> idx(lists.size(), 0);
    std::priority_queue<PUU, std::vector<PUU>, std::greater<PUU>> pq;
    unsigned total_size = 0;
    for (unsigned i = 0; i < lists.size(); ++i) {
        if (idx[i] < sizes[i]) {
            pq.push({lists[i][idx[i]], i});
            total_size += sizes[i];
        }
    }
    result.reserve(total_size);
    while (!pq.empty()) {
        PUU top = pq.top();
        pq.pop();
        if (result.empty() || result.back() != top.first) {
            result.push_back(top.first);
        }
        if (idx[top.second] + 1 < sizes[top.second]) {
            pq.push({lists[top.second][++idx[top.second]], top.second});
        }
    }
}

void TwoListsMerge(const std::vector<unsigned>& list1, const std::vector<unsigned>& list2, std::vector<unsigned>& result) {
    std::vector<unsigned*> lists = {const_cast<unsigned*>(list1.data()), const_cast<unsigned*>(list2.data())};
    std::vector<unsigned> sizes = {(unsigned int)list1.size(), (unsigned int)list2.size()};
    MultilistMerge(lists, sizes, result);
}

// bool CheckHomomorphism(std::vector<unsigned>& query_node_seq,
//                        std::vector<unsigned>& match_node_in_query_node_seq,
//                        QueryGraphAdapter* query_graph,
//                        DataGraphAdapter* data_graph) {
//   std::map<unsigned, unsigned> match_mapping;
//   for (unsigned i = 0; i < query_node_seq.size(); ++i) {
//     match_mapping[query_node_seq[i]] = match_node_in_query_node_seq[i];
//   }
//   for (unsigned i = 0; i < query_graph->GetEdgeNum(); ++i) {
//     unsigned src = query_graph->GetSrc(i);
//     unsigned dst = query_graph->GetDst(i);
//     unsigned src_match = match_mapping[src];
//     unsigned dst_match = match_mapping[dst];
//     if (!data_graph->CheckEdgeExistence(src_match, dst_match)) {
//       std::cout << "Edge " << src << " -> " << dst
//                 << " not exist in data graph\n";
//       std::cout << "Matched nodes: " << src_match << " -> " << dst_match
//                 << std::endl;
//       return false;
//     }
//   }
//   return true;
// }

void MultilistInsection(std::vector<unsigned*>& lists, std::vector<unsigned>& sizes, std::vector<unsigned>& result) {
    // leapfrog join
    for (unsigned i = 0; i < lists.size(); ++i) {
        for (unsigned j = 0; j < i; ++j) {
            if (lists[i][0] < lists[j][0]) {
                std::swap(lists[i], lists[j]);
                std::swap(sizes[i], sizes[j]);
            }
        }
    }
    std::vector<unsigned> curs(lists.size(), 0);
    unsigned max_val = lists.back()[0];
    unsigned idx = 0;
    for (;;) {
        unsigned min_val = lists[idx][curs[idx]];
        if (min_val == max_val) {
            result.push_back(min_val);
            curs[idx]++;
            if (curs[idx] >= sizes[idx]) return;
            max_val = lists[idx][curs[idx]];
            idx = (idx + 1) % lists.size();
        } else {
            curs[idx] = std::lower_bound(lists[idx] + curs[idx], lists[idx] + sizes[idx], max_val) - lists[idx];
            if (curs[idx] >= sizes[idx]) return;
            max_val = lists[idx][curs[idx]];
            idx = (idx + 1) % lists.size();
        }
    }
}

bool NoRepeat(unsigned* embedding, unsigned len) {
    for (unsigned i = 0; i < len; ++i) {
        for (unsigned j = i + 1; j < len; ++j) {
            if (embedding[i] == embedding[j]) {
                return false;
            }
        }
    }
    return true;
}

std::vector<int> BitToVector(unsigned x, unsigned len) {
    std::vector<int> ret;
    for (unsigned i = 0; i < len; ++i) {
        if (x & (1u << i)) {
            ret.push_back(i);
        }
    }
    return ret;
}

bool CheckHomomorphism(Graph* query_graph, Graph* data_graph, const std::vector<unsigned>& node_seq, const unsigned* embedding) {
    for (int i = 0; i < query_graph->getVerticesCount(); i++) {
        for (int j = i + 1; j < query_graph->getVerticesCount(); j++) {
            if (query_graph->checkEdgeExistence(node_seq[i], node_seq[j])) {
                if (!data_graph->checkEdgeExistence(embedding[i], embedding[j])) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool CheckHomomorphism(Graph* query_graph, Graph* data_graph, const std::vector<unsigned>& node_seq, const std::vector<unsigned>& embedding) {
    return CheckHomomorphism(query_graph, data_graph, node_seq, embedding.data());
}

int QuickLog2(unsigned x) {
    return 32 - __builtin_clz(x - 1);
}

unsigned long long Intersection(unsigned* a, unsigned a_n, unsigned* b, unsigned b_n, int* idxs, int& idxs_n) {
    idxs_n = 0;
    unsigned long long cost = 0;
    unsigned long long costa = a_n * QuickLog2(b_n + 1);
    unsigned long long costb = b_n * QuickLog2(a_n + 1);
    unsigned long long costc = a_n + b_n;
    if (costa < costc && costa < costb) {
        for (int i = 0; i < a_n; ++i) {
            if (std::binary_search(b, b + b_n, a[i])) {
                idxs[idxs_n++] = i;
            }
        }
        cost = costa;
    } else if (costb < costc) {
        for (int i = 0; i < b_n; ++i) {
            auto idx = std::lower_bound(a, a + a_n, b[i]) - a;
            if (idx < a_n && a[idx] == b[i]) {
                idxs[idxs_n++] = idx;
            }
        }
        cost = costb;
    } else {
        int idx0 = 0, idx1 = 0;
        while (idx0 < a_n && idx1 < b_n) {
            if (a[idx0] == b[idx1]) {
                // idxs.push_back(idx0);
                idxs[idxs_n++] = idx0;
                idx0++;
                idx1++;
            } else if (a[idx0] < b[idx1]) {
                idx0++;
            } else {
                idx1++;
            }
        }
        cost = costc;
    }
    return cost;
}

std::string GetRandomColors() {
    std::string color = "#";
    for (unsigned j = 0; j < 6; ++j) {
        char tmp[5];
        sprintf(tmp, "%x", rand() % 13);
        color += tmp;
    }
    return color;
}


void GetKeyByPos(const unsigned* embedding, const std::vector<int>& positions, std::vector<int>& key) {
    key.clear();
    for (auto pos : positions) {
        key.push_back(embedding[pos]);
    }
}

bool ContainSameValue(unsigned* emb1, unsigned len1, unsigned* emb2, unsigned len2) {
    for (unsigned i = 0; i < len1; ++i) {
        for (unsigned j = 0; j < len2; ++j) {
            if (emb1[i] == emb2[j]) {
                return true;
            }
        }
    }
    return false;
}