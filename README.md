# MAVIS

Subgraph matching is a fundamental task in graph analysis systems. In real-world applications, graph query engines often need to process a large number of subgraph matching queries, many of which share common substructures. Materializing the results of these shared subqueries as view patterns can enable computation reuse and significantly improve the efficiency of future queries. However, existing view materialization techniques suffer from either high memory usage or limited acceleration benefits. This paper introduces MAVIS, a novel view-based subgraph matching algorithm. MAVIS partitions view patterns into connected subgraphs called super-nodes and performs super-node-oriented materialization to balance memory consumption and processing speed. To further improve efficiency, it proposes a tree-based super-node partitioning method that avoids generating invalid candidates during materialization. Additionally, a customized query answering algorithm leveraging the materialized views is designed, which incorporates a novel optimization to eliminate redundant searches. Furthermore, we propose an incremental maintenance algorithm to update views upon data changes without full recomputation. Extensive experiments on real-world datasets demonstrate that MAVIS achieves a superior trade-off between memory usage and acceleration, and it outperforms existing approaches.

## Build

Our project is built on cmake. Under the root directory of the project, execute the following commands to compile the source code.

``` shell
mkdir build
cd build
cmake ..
make -j
```

## Usage

After compiling the source code, you can find the binary file 'mavis' under the 'build' directory. 

There are three types of commands in mavis:

* Test:
  The following command is used to test the basic function of mavis. 

``` shell
build/mavis -test
```


* View Construction:
  The following command is used to build materialized views. The parameter `-data` specifies the path of the data graph. The parameter `-view` specifies the paths of view patterns. If there are multiple views that need materializing, seperating them by ",". The parameter `-mv_path` specifies the folder in which to the materialized views. Make sure the folder exists before running the command.

``` shell
build/mavis -data [data_graph_path] -view [view_1,...,view_n] -mv_path [matview_folder]
```

* Answer Query:
  The following command is used to answer a query. Here, `-data` and `-query` specify the path of the data graph and the query graph, respectively. `-mv_path` specifies the folder that stores the materialized views. The program will accelerate the query processing by the materialized views when they are available.

``` shell
build/mavis -data [data_graph_path] -query [query_graph_path] -mv_path [matview_folder]
```

## Example

There are two view patterns and a query graph in `dataset/test1/example`. They are the subgraphs of the data graph located in `dataset/test1/data_graph/HPRD.graph`. These views are also the subgraphs of the query graph. We can use them to answer the query graph.

1. Materialiation
   Execute the following command to materialize the views:

``` shell
build/mavis -data dataset/test1/data_graph/HPRD.graph -view dataset/test1/example/view_pattern_0.graph,dataset/test1/example/view_pattern_1.graph -mv_path materialization
```

2. Answer Query

``` shell
build/mavis -data dataset/test1/data_graph/HPRD.graph -query dataset/test1/example/query_graph.graph -mv_path materialization
```

## Reference

Our project utilizes the source code from RapidMatch. This is its github link: [https://github.com/RapidsAtHKUST/RapidMatch][https://github.com/RapidsAtHKUST/RapidMatch]. And its related paper is "Shixuan Sun, Xibo Sun, Yulin Che, Qiong Luo, and Bingsheng He. RapidMatch: A Holistic Approach to Subgraph Query Processing. VLDB 2021."