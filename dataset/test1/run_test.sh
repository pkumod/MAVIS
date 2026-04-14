#!/bin/bash
# Test script for MAVIS
# Run from the project root directory

set -e

MAVIS="./build/mavis"
DATA_GRAPH="dataset/test1/data_graph/HPRD.graph"
MV_PATH="materialization"

echo "=============================="
echo "  MAVIS Test Script"
echo "=============================="

# 0. Build check
if [ ! -f "$MAVIS" ]; then
    echo "[BUILD] mavis binary not found, building..."
    mkdir -p build && cd build && cmake .. && make -j$(nproc) && cd ..
fi

# 1. Test (-test)
echo ""
echo "[TEST 1] Running -test ..."
$MAVIS -test

# 2. View Construction (materialization)
echo ""
echo "[TEST 2] Materializing views ..."
mkdir -p "$MV_PATH"
$MAVIS -data "$DATA_GRAPH" \
       -view "dataset/test1/example/view1.graph,dataset/test1/example/view2.graph" \
       -mv_path "$MV_PATH"

# 3. Answer Query
echo ""
echo "[TEST 3] Answering query with materialized views ..."
$MAVIS -data "$DATA_GRAPH" \
       -query "dataset/test1/example/query_graph.graph" \
       -mv_path "$MV_PATH"

# Cleanup
rm -rf "$MV_PATH"

echo ""
echo "=============================="
echo "  All tests passed!"
echo "=============================="
