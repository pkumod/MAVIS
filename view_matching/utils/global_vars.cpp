#include "global_vars.h"

long long g_build_view_cost = 0;

long long g_rewrite_cost = 0;

long long g_load_cost = 0;

long long g_preprocess_cost = 0;


long long g_encode_cost = 0;
long long g_rapidmatch_encode_cost = 0;
long long g_my_encode_cost = 0;

long long g_enumerate_cost = 0;

bool g_exit = false;

bool g_use_naive_partition = false;

bool g_merge_super_node = false;

bool g_port_vertex_optimization = true;


std::shared_ptr<std::string> log_str = nullptr;