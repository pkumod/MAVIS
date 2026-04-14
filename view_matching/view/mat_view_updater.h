#include "mat_view.h"
#include "mat_view_builder.h"

class MatViewUpdater {
   private:
    MatView& mat_view_;
    Graph *query_graph_, *data_graph_;
    std::unique_ptr<SubgraphFilter> filter_;

    std::vector<ui> flag_;
    ui flag_cnt_ = 0;

    int TransEmbeddingId(SuperDataNode& src_sdn, SuperDataNode& dst_sdn, int embedding_id);

    void AddGroupEdge(ui u_gid, ui v_gid, SuperDataEdge& sde, int direction);

    void BatchAddGroupEdge(std::unordered_map<ui, std::vector<ui>>& group_edges, SuperDataEdge& sde, int direction);

    void CascadeDelete(ui su, ui embedding_id);

    void DeleteSuperDataEdge(ui su, ui sv, ui u_gid, ui v_gid, MatView& imat_view);

    bool CheckEmbedding(ui su, ui embedding_id);

    void BatchGetEmbeddingIds(
        int cur_layer,
        int cur_node_id,
        SuperDataNode& dst_sdn, SuperDataNode& ori_sdn,
        std::vector<int>& embedding_ids, int l, int r);

    ui complete_cs_cnt_ = 0;
    ui complete_mat_view_cnt_ = 0;
    ui rebuild_cnt_ = 0;

   public:
    // data structures for deletion
    std::vector<std::pair<ui, ui>> matched_edges_;

   public:
    MatViewUpdater(MatView& mat_view, Graph* query_graph, Graph* data_graph) : mat_view_(mat_view), query_graph_(query_graph), data_graph_(data_graph) {
        filter_ = std::make_unique<SubgraphFilter>(query_graph_, data_graph_);
        flag_.resize(3000000);
        flag_.assign(3000000, 0);
    }

    void BuildFilterResultFromEdge(CandidateSpace& cs, ui du, ui dv, bool record_matched_edge = false);
    void BuildFilterResultFromEdgeBatch(CandidateSpace& cs, std::vector<std::pair<ui, ui>>& edge_list);

    void InsertEdge(ui u, ui v, Graph* data_graph);
    void InsertEdgeBatch(std::vector<std::pair<ui, ui>>& edges, Graph* data_graph);

    void InsertFromCS(CandidateSpace& cs, Graph* data_graph);

    void DeleteEdge(ui u, ui v, Graph* data_graph);
    void DeleteEdgeBatch(std::vector<std::pair<ui, ui>>& edges, Graph* data_graph);

    void Statistics();
};