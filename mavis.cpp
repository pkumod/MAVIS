#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "utility/pretty_print.h"
#include "graph/graph.h"
#include "view_matching/view/mat_view_builder.h"
#include "view_matching/answer/rewrite_query.h"

// 
// materilization command: ./mavis -data [data_graph] -view [v1,v2,...] -mv_path [view_path]
// query answer command: ./mavis -data [data_graph] -query [query_graph] -mv_path [view_path]
// test command: ./mavis -test

void HandleInvalidCommand() {
    std::cout << "Invalid command. Please use the following format:" << std::endl;
    std::cout << "To run tests: ./mavis -test" << std::endl;
    std::cout << "To materialize views: ./mavis -data [data_graph] -view [v1,v2,...] -mv_path [view_path]" << std::endl;
    std::cout << "To answer a query: ./mavis -data [data_graph] -query [query_graph] -mv_path [view_path]" << std::endl;
    exit(-1);
}

void Materialize(std::string data_graph_path, std::vector<std::string> view_list, std::string mv_path) {
    std::unique_ptr<Graph> data_graph(new Graph(true));
    data_graph->loadGraphFromFile(data_graph_path);
    int cnt = 0;
    for (auto view_path : view_list) {
        std::unique_ptr<Graph> view_graph(new Graph(true));
        view_graph->loadGraphFromFile(view_path);
        MatView res;
        MatViewBuilder mv_builder;
        mv_builder.Build(view_graph.get(), data_graph.get(), res);
        res.Dump(mv_path + "/matview" + std::to_string(cnt) + ".txt");
        ++cnt;
    }
}

void Answer(std::string data_graph_path, std::string query_path, std::string mv_path) {
    std::unique_ptr<Graph> data_graph(new Graph(true));
    data_graph->loadGraphFromFile(data_graph_path);

    std::unique_ptr<Graph> query_graph(new Graph(true));
    query_graph->loadGraphFromFile(query_path);

    // load mat view
    std::vector<MatView> mat_views;
    int cnt = 0;
    while (true) {
        MatView mv;
        mv.file_name_ = mv_path + "/matview" + std::to_string(cnt) + ".txt";

        std::ifstream infile(mv.file_name_);
        if (!infile.good()) break;
        infile.close();

        mv.Load();
        mat_views.push_back(std::move(mv));
        ++cnt;
    }

    // answer query using mat views
    ProcessQueryResult result = ProcessQuery(data_graph.get(), query_graph.get(), mat_views, 100000, 60 * 5);
    std::cout << "embedding cnt: " << result.embedding_num << std::endl;
    std::cout << "throughput: " << (double)result.embedding_num / result.enum_cost * 1000000 << " (eps)\n";
    std::cout << "total cost(ms): " << (result.enum_cost + result.encode_cost + result.preprocess_cost + result.rewrite_cost) / 1000. << std::endl;
    std::cout << "\tenum cost: " << result.enum_cost / 1000. << " ms" << std::endl;
    std::cout << "\tencode cost: " << result.encode_cost / 1000. << " ms" << std::endl;
    std::cout << "\tpreprocess cost: " << result.preprocess_cost / 1000. << " ms" << std::endl;
    std::cout << "\trewrite cost: " << result.rewrite_cost / 1000. << " ms" << std::endl;
}

void LoadAnswer(std::vector<unsigned> &answers) {
    std::string answer_path = "./dataset/test1/expected_output.res";
    std::ifstream answer_file(answer_path);
    std::string tmp;
    while (answer_file >> tmp) {
        for (unsigned i = 0; i < tmp.size(); ++i) {
            if (tmp[i] == ':') {
                unsigned answer = 0;
                for (unsigned j = i + 1; j < tmp.size(); ++j) {
                    answer = answer * 10 + tmp[j] - '0';
                }
                answers.push_back(answer);
                break;
            }
        }
    }
    answer_file.close();
}

void Test() {
    auto data_graph_path = "./dataset/test1/data_graph/HPRD.graph";
    std::unique_ptr<Graph> data_graph(new Graph(true));
    data_graph->loadGraphFromFile(data_graph_path);
    std::vector<unsigned> answers;
    LoadAnswer(answers);
    bool pass = true;
    for (int i = 1; i <= 200; ++i) {
        auto query_graph_path = "./dataset/test1/query_graph/query_dense_16_" + std::to_string(i) + ".graph";
        auto view_pattern_path = "./dataset/test1/query_subgraph/subquery_dense_16_" + std::to_string(i) + ".graph";
        std::unique_ptr<Graph> query_graph(new Graph(true));
        query_graph->loadGraphFromFile(query_graph_path);
        std::unique_ptr<Graph> view_pattern(new Graph(true));
        view_pattern->loadGraphFromFile(view_pattern_path);

        std::vector<MatView> mv_set;
        MatView mv;
        MatViewBuilder mv_builder;
        bool have_answer = mv_builder.Build(view_pattern.get(), data_graph.get(), mv);
        if (have_answer == false) {
            std::cout << "failed to build view for query no." << i << std::endl;
            pass = false;
            break;
        }
        mv_set.push_back(std::move(mv));

        mv_set.front().Dump("./test_dump");
        MatView mv2;
        mv2.file_name_ = "./test_dump";
        mv2.Load();
        mv_set.front() = std::move(mv2);

        std::cout << "query no." << i << std::endl;
        ProcessQueryResult result;
        result = ProcessQuery(data_graph.get(), query_graph.get(), mv_set, 1e10, 10);
        if (result.embedding_num != answers[i - 1]) {
            std::cout << "expected: " << answers[i - 1] << ", got: " << result.embedding_num << std::endl;
            pass = false;
            break;
        }
    }
    std::remove("./test_dump");
    if (pass) std::cout <<"All test cases passed!" << std::endl;
    else exit(1);
}

int main(int argc, char** argv) {
    std::map<std::string, std::string> params;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-') {
            std::string key = arg.substr(1);
            if (i + 1 < argc) {
                std::string value = argv[i + 1];
                if (value[0] != '-') {
                    params[key] = value;
                    i++;
                } else {
                    params[key] = "1";
                }
            } else {
                params[key] = "1";
            }
        }
    }

    if (params.count("test")) {
        Test();
        return 0;
    }

    if (params.count("data") == 0 || params.count("mv_path") == 0) HandleInvalidCommand();
    auto data_graph_path = params["data"];
    auto mv_path = params["mv_path"];

    if (params.count("query")) { // answer query
        Answer(data_graph_path, params["query"], mv_path);
    }
    else if (params.count("view")) { // materialize view
        auto view_list_str = params["view"];
        std::vector<std::string> view_list;
        size_t pos = 0;
        while ((pos = view_list_str.find(',')) != std::string::npos) {
            view_list.push_back(view_list_str.substr(0, pos));
            view_list_str.erase(0, pos + 1);
        }
        view_list.push_back(view_list_str);
        Materialize(data_graph_path, view_list, mv_path);
    }
    else HandleInvalidCommand();

    return 0;
}