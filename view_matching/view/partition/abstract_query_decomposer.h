#ifndef ABSTRACT_QUERY_DECOMPOSER_H
#define ABSTRACT_QUERY_DECOMPOSER_H

#include <fstream>
#include "MyUtils.h"

class AbstractPartitioner {

public:
    AbstractPartitioner() {}

    virtual void QueryDecomposition(std::vector<std::vector<ui>>& res) = 0;
    virtual ~AbstractPartitioner() {}
};

#endif