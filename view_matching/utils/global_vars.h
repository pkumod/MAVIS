#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include <string>
#include <memory>
#include <atomic>

#ifdef ENABLE_DOT_OUTPUT
#include <sys/stat.h>
#include <fstream>
#include <iostream>

inline void EnsureDotDir() {
    const char* dir = "dot_file";
    struct stat st;
    if (stat(dir, &st) != 0) {
        mkdir(dir, 0755);
    }
}

inline std::ofstream OpenDotFile(const std::string& filename) {
    EnsureDotDir();
    return std::ofstream("dot_file/" + filename);
}
#endif


// view
extern long long g_build_view_cost;

// rewrite
extern long long g_rewrite_cost; 

extern long long g_load_cost;

extern long long g_preprocess_cost;

extern long long g_encode_cost;
extern long long g_rapidmatch_encode_cost;
extern long long g_my_encode_cost;

// enumerate
extern long long g_enumerate_cost;

extern bool g_exit;
extern bool g_use_naive_partition;

extern bool g_merge_super_node;

extern std::shared_ptr<std::string> log_str;

const int MAX_EMBEDDING_NUM = 1e6;
const int MAX_EDGE_NUM = 1e8;

extern bool g_oversized;

extern bool g_port_vertex_optimization;

#endif //GLOBAL_VARS_H