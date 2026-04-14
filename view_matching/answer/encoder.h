#ifndef SUBGRAPHMATCHING_ENCODER_H
#define SUBGRAPHMATCHING_ENCODER_H

#include <relation/catalog.h>

#include "enumerate.h"
#include "rewrite_query.h"

class Encoder {
   public:
    double encoding_time_;
    double build_reverse_index_time_;
    AuxilaryInfo& aux_;
    RewriteQuery& rq_;

   private:
    uint32_t max_vertex_id_;
    uint32_t* join_plan_;
    uint32_t* temp_buffer1_;

   private:
    void convert_to_encoded_relation(catalog* storage, uint32_t u, uint32_t v);

   public:
    Encoder(const Graph* query_graph, uint32_t max_vertex_id, AuxilaryInfo& aux, RewriteQuery& rq)
        : encoding_time_(0), max_vertex_id_(max_vertex_id), aux_(aux), rq_(rq) {
        temp_buffer1_ = new uint32_t[max_vertex_id_];
        memset(temp_buffer1_, 0, max_vertex_id_ * sizeof(uint32_t));
    }

    void execute();

    ~Encoder() {
        delete[] temp_buffer1_;
    }
};

#endif  // SUBGRAPHMATCHING_ENCODER_H
