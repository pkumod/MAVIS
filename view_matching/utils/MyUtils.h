#ifndef MY_UTIL_H
#define MY_UTIL_H

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <vector>

// #include "DataGraphAdapter.h"
// #include "QueryGraphAdapter.h"

#include "graph.h"

#define PRINT_RED(...)        \
    do {                      \
        printf("\033[1;31m"); \
        printf(__VA_ARGS__);  \
        printf("\033[0m");    \
    } while (0)

#define PRINT_GREEN(...)      \
    do {                      \
        printf("\033[1;32m"); \
        printf(__VA_ARGS__);  \
        printf("\033[0m");    \
    } while (0)

#define PRINT_YELLOW(...)     \
    do {                      \
        printf("\033[1;33m"); \
        printf(__VA_ARGS__);  \
        printf("\033[0m");    \
    } while (0)

#define PRINT_BLUE(...)       \
    do {                      \
        printf("\033[1;34m"); \
        printf(__VA_ARGS__);  \
        printf("\033[0m");    \
    } while (0)

#define MICROSECOND2SECOND(x) ((x) / 1000000.0)

// template function: check if a value is in a sorted vector, if true, return
// the position, else return -1
template <class T>
int GetPosInOrderList(const std::vector<T>& vec, const T& val) {
    auto pos = std::lower_bound(vec.begin(), vec.end(), val) - vec.begin();
    return pos < vec.size() && vec[pos] == val ? pos : -1;
}

template <class T>
int GetPos(const std::vector<T>& vec, const T& val) {
    auto pos = std::find(vec.begin(), vec.end(), val);
    return pos == vec.end() ? -1 : pos - vec.begin();
}

// template function: check if a value is in a sorted vector
template <class T>
bool IsIn(const std::vector<T>& vec, const T& val) {
    return GetPosInOrderList(vec, val) != -1;
}

// template function: move array to a vector
template <class T>
void CopyArrayToVector(const T* arr, unsigned arr_size, std::vector<T>& vec) {
    vec.resize(arr_size);
    memcpy(vec.data(), arr, sizeof(T) * arr_size);
}

template <class T>
void CopyArrayToVector(std::unique_ptr<T[]>& arr, unsigned arr_size, std::vector<T>& vec) {
    vec.resize(arr_size);
    memcpy(vec.data(), arr.get(), sizeof(T) * arr_size);
}

// template function: intersect two sorted vectors, store into vector
template <class T>
void IntersectSortedLists(std::vector<T>& e, const std::vector<T>& f) {
    std::vector<T> c;
    const auto& a = e.size() < f.size() ? e : f;
    const auto& b = e.size() < f.size() ? f : e;
    auto num_a = a.size(), num_b = b.size();
    if (num_a * (log(num_b + 1) / log(2)) > num_a + num_b) {
        int i0 = 0, i1 = 0;
        while (i0 < a.size() && i1 < b.size()) {
            if (a[i0] == b[i1]) {
                c.push_back(a[i0]);
                i0++;
                i1++;
            } else if (a[i0] < b[i1]) {
                i0++;
            } else {
                i1++;
            }
        }
    } else {
        for (auto x : a) {
            if (IsIn(b, x)) {
                c.push_back(x);
            }
        }
    }
    e = c;
}

template <class T>
void MergeTwoSortedLists(const std::vector<T>& a, const std::vector<T>& b, std::vector<T>& c) {
    int i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            c.push_back(a[i]);
            i++;
            j++;
        } else if (a[i] < b[j]) {
            c.push_back(a[i]);
            i++;
        } else {
            c.push_back(b[j]);
            j++;
        }
    }
    if (i < a.size()) c.insert(c.end(), a.begin() + i, a.end());
    if (j < b.size()) c.insert(c.end(), b.begin() + j, b.end());
}

// template function: insect an array and a vector, store into vector. The array
// and vector are sorted.
template <class T>
void IntersectArrayToVector(const T* arr, unsigned arr_size, std::vector<T>& vec) {
    std::vector<T> c;
    CopyArrayToVector(arr, arr_size, c);
    IntersectSortedLists(vec, c);
}

template <class T>
void IntersectArrayToVector(std::unique_ptr<T[]>& arr, unsigned arr_size, std::vector<T>& vec) {
    std::vector<T> c;
    CopyArrayToVector(arr, arr_size, c);
    IntersectSortedLists(vec, c);
}

// my assert, output line number and function name
#define ASSERT(x)                                                            \
    if (!(x)) {                                                              \
        std::cout << "ASSERT FAILED: " << __FILE__ << ":" << __LINE__ << " " \
                  << __FUNCTION__ << std::endl;                              \
        exit(1);                                                             \
    }

// template class for union-find
template <class T>
class UnionFind {
    std::map<T, T> fa_;

   public:
    UnionFind() = default;
    UnionFind(UnionFind<T>&& uf) = default;
    UnionFind(const UnionFind<T>& uf) = default;
    UnionFind& operator=(UnionFind<T>&& uf) = default;
    UnionFind& operator=(const UnionFind<T>& uf) = default;

    // only for continuous integer
    UnionFind(int num) {
        for (int i = 0; i < num; ++i) {
            fa_[i] = i;
        }
    }

    UnionFind(const std::vector<T>& nodes) {
        for (auto node : nodes) {
            fa_[node] = node;
        }
    }

    T Find(T x) { return fa_[x] == x ? x : fa_[x] = Find(fa_[x]); }
    void Union(T x, T y) { fa_[Find(x)] = Find(y); }
};

unsigned MyHash(unsigned x);

typedef std::pair<unsigned, unsigned> PUU;

void MultilistMerge(const std::vector<unsigned*>& lists, const std::vector<unsigned>& sizes, std::vector<unsigned>& result);

void MultilistMerge(const std::vector<const unsigned*>& lists, const std::vector<unsigned>& sizes, std::vector<unsigned>& result);

void MultilistInsection(std::vector<unsigned*>& lists, std::vector<unsigned>& sizes, std::vector<unsigned>& result);

void TwoListsMerge(const std::vector<unsigned>& list1, const std::vector<unsigned>& list2, std::vector<unsigned>& result);

bool CheckHomomorphism(Graph* query_graph, Graph* data_graph, const std::vector<unsigned>& node_seq, const unsigned* embedding);

bool CheckHomomorphism(Graph* query_graph, Graph* data_graph, const std::vector<unsigned>& node_seq, const std::vector<unsigned>& embedding);

bool NoRepeat(unsigned* embedding, unsigned len);

std::vector<int> BitToVector(unsigned x, unsigned len);

unsigned long long Intersection(unsigned* a, unsigned a_n, unsigned* b, unsigned b_n, int* idxs, int& idxs_n);

std::string GetRandomColors();

void GetKeyByPos(const unsigned* embedding, const std::vector<int>& positions, std::vector<int>& key);

template<class T>
void EraseValueFromSortedVector(std::vector<T>& vec, const T& val) {
    auto pos = std::lower_bound(vec.begin(), vec.end(), val);
    assert(pos != vec.end() && *pos == val);
    vec.erase(pos);
}

template<class T>
void EraseValueFromVector(std::vector<T>& vec, const T& val) {
    auto pos = std::find(vec.begin(), vec.end(), val);
    assert(pos != vec.end() && *pos == val);
    vec.erase(pos);
}

bool ContainSameValue(unsigned* emb1, unsigned len1, unsigned* emb2, unsigned len2); 

int QuickLog2(unsigned x);

#endif