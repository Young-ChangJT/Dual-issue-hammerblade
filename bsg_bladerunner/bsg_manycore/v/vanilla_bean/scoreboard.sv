/**
 * scoreboard.v
 *
 * 2020-05-08:  Tommy J - adding FMA support.
 * 2026-02-25:  Modified for Dual-issue support (2 Score Ports).
 */

`include "bsg_defines.sv"

module scoreboard
  import bsg_vanilla_pkg::*;
  #(els_p = RV32_reg_els_gp
    , `BSG_INV_PARAM(num_src_port_p)
    , num_clear_port_p=1
    , parameter num_score_port_p = 2 // 新增：參數化 Score Port 數量，預設為 2
    , x0_tied_to_zero_p = 0
    , localparam id_width_lp = `BSG_SAFE_CLOG2(els_p)
  )
  (
    input clk_i
    , input reset_i

    , input [num_src_port_p-1:0][id_width_lp-1:0] src_id_i
    , input [id_width_lp-1:0] dest_id_i

    , input [num_src_port_p-1:0] op_reads_rf_i
    , input op_writes_rf_i

    /* 原本的單埠 Score 定義
    , input score_i
    , input [id_width_lp-1:0] score_id_i
    */
    // 新增：雙埠 Score 定義
    , input [num_score_port_p-1:0] score_i
    , input [num_score_port_p-1:0][id_width_lp-1:0] score_id_i

    , input [num_clear_port_p-1:0] clear_i
    , input [num_clear_port_p-1:0][id_width_lp-1:0] clear_id_i

    , output logic dependency_o
  );

  logic [els_p-1:0] scoreboard_r;

  // multi-port clear logic
  //
  logic [num_clear_port_p-1:0][els_p-1:0] clear_by_port;
  logic [els_p-1:0][num_clear_port_p-1:0] clear_by_port_t; // transposed
  logic [els_p-1:0] clear_combined;

  bsg_transpose #(
    .els_p(num_clear_port_p)
    ,.width_p(els_p)
  ) tranposer (
    .i(clear_by_port)
    ,.o(clear_by_port_t)
  );

  for (genvar j = 0 ; j < num_clear_port_p; j++) begin: clr_dcode_v
    bsg_decode_with_v #(
      .num_out_p(els_p)
    ) clear_decode_v (
      .i(clear_id_i[j])
      ,.v_i(clear_i[j])
      ,.o(clear_by_port[j])
    );
  end

  always_comb begin
    for (integer i = 0; i < els_p; i++) begin
      clear_combined[i] = |clear_by_port_t[i];
    end
  end


  // synopsys translate_off
  always_ff @ (negedge clk_i) begin
    if (~reset_i) begin
      for (integer i = 0; i < els_p; i++) begin
        assert($countones(clear_by_port_t[i]) <= 1) else
          $error("[ERROR][SCOREBOARD] multiple clear on the same id. t=%0t", $time);
      end
    end
  end
  // synopsys translate_on

  /* 原本的單埠 Score 解碼邏輯 (註解保留)
  wire allow_zero = (x0_tied_to_zero_p == 0) | (score_id_i != '0);

  logic [els_p-1:0] score_bits;
  bsg_decode_with_v #(
    .num_out_p(els_p)
  ) score_demux (
    .i(score_id_i)
    ,.v_i(score_i & allow_zero)
    ,.o(score_bits)
  );
  */

  // 新增：多路 Score 解碼邏輯 (與 Clear 邏輯對稱)
  logic [num_score_port_p-1:0][els_p-1:0] score_by_port;
  logic [els_p-1:0][num_score_port_p-1:0] score_by_port_t;
  logic [els_p-1:0] score_combined;

  for (genvar k = 0; k < num_score_port_p; k++) begin: score_dcode_v
    wire allow_zero_multi = (x0_tied_to_zero_p == 0) | (score_id_i[k] != '0);
    bsg_decode_with_v #(
      .num_out_p(els_p)
    ) score_decode_multi (
      .i(score_id_i[k])
      ,.v_i(score_i[k] & allow_zero_multi)
      ,.o(score_by_port[k])
    );
  end

  bsg_transpose #(
    .els_p(num_score_port_p)
    ,.width_p(els_p)
  ) score_tranposer (
    .i(score_by_port)
    ,.o(score_by_port_t)
  );

  always_comb begin
    for (integer i = 0; i < els_p; i++) begin
      score_combined[i] = |score_by_port_t[i]; // 合併多個 Score Port 的結果
    end
  end

  always_ff @ (posedge clk_i) begin
    for (integer i = 0; i < els_p; i++) begin
      if(reset_i) begin
        scoreboard_r[i] <= 1'b0;
      end
      else begin
        // "score" takes priority over "clear" in case of 
        // simultaneous score and clear. But this
        // condition should not occur in general, as 
        // the pipeline should not allow a new dependency
        // on a register until the old dependency on that 
        // register is cleared.

        /* 原本的更新邏輯 (註解保留)
        if(score_bits[i]) begin
          scoreboard_r[i] <= 1'b1;
        end
        */
        // 新增：使用合併後的 score_combined 更新狀態
        if(score_combined[i]) begin
          scoreboard_r[i] <= 1'b1;
        end
        else if (clear_combined[i]) begin
          scoreboard_r[i] <= 1'b0;
        end
      end
    end
  end

 
  // dependency logic
  // As the register is scored (in EXE), the instruction in ID that has WAW or RAW dependency on this register stalls.
  // The register that is being cleared does not stall ID. 

  // find dependency on scoreboard.
  logic [num_src_port_p-1:0] rs_depend_on_sb;
  logic rd_depend_on_sb;

  for (genvar i = 0; i < num_src_port_p; i++) begin
    assign rs_depend_on_sb[i] = scoreboard_r[src_id_i[i]] & op_reads_rf_i[i];
  end
  
  assign rd_depend_on_sb = scoreboard_r[dest_id_i] & op_writes_rf_i;

  // find which matches on clear_id.
  logic [num_clear_port_p-1:0][num_src_port_p-1:0] rs_on_clear;
  logic [num_src_port_p-1:0][num_clear_port_p-1:0] rs_on_clear_t;
  logic [num_clear_port_p-1:0] rd_on_clear;
  
  for (genvar i = 0; i < num_clear_port_p; i++) begin
    for (genvar j = 0; j < num_src_port_p; j++) begin
      assign rs_on_clear[i][j] = clear_i[i] && (clear_id_i[i] == src_id_i[j]);
    end

    assign rd_on_clear[i] = clear_i[i] && (clear_id_i[i] == dest_id_i);
  end

  bsg_transpose #(
    .els_p(num_clear_port_p)
    ,.width_p(num_src_port_p)
  ) trans1 (
    .i(rs_on_clear)
    ,.o(rs_on_clear_t)
  );

  logic [num_src_port_p-1:0] rs_on_clear_combined;
  logic rd_on_clear_combined;

  for (genvar i = 0; i < num_src_port_p; i++) begin
    assign rs_on_clear_combined[i] = |rs_on_clear_t[i];
  end

  assign rd_on_clear_combined = |rd_on_clear;

  // find which could depend on score.

  /* 原本的單埠依賴檢查 (註解保留)
  logic [num_src_port_p-1:0] rs_depend_on_score;
  logic rd_depend_on_score;

  for (integer i = 0; i < num_src_port_p; i++) begin
    assign rs_depend_on_score[i] = (src_id_i[i] == score_id_i) && op_reads_rf_i[i];
  end

  assign rd_depend_on_score = (dest_id_i == score_id_i) && op_writes_rf_i;
  */

  // 新增：多埠依賴檢查 (檢查 src_id 是否與任何一個 Score Port 的 score_id 相同)
  logic [num_src_port_p-1:0] rs_depend_on_score_any;
  logic rd_depend_on_score_any;
  logic [num_src_port_p-1:0] match_any_rs;
  logic match_any_rd;

  always_comb begin
    match_any_rs = '0;
    match_any_rd = 1'b0;

    for (integer i = 0; i < num_src_port_p; i++) begin
      for (integer k = 0; k < num_score_port_p; k++) begin
        if (x0_tied_to_zero_p == 0 || score_id_i[k] != '0) begin
          match_any_rs[i] |= score_i[k] & (src_id_i[i] == score_id_i[k]);
        end
      end
    end

    for (integer k = 0; k < num_score_port_p; k++) begin
      if (x0_tied_to_zero_p == 0 || (score_id_i[k] != '0)) begin
      match_any_rd |= score_i[k] & (dest_id_i == score_id_i[k]);
    end
    end

    for (integer i = 0; i < num_src_port_p; i++) begin
      rs_depend_on_score_any[i] = match_any_rs[i] & op_reads_rf_i[i];
    end
    rd_depend_on_score_any = match_any_rd & op_writes_rf_i;
  end

  // score_i arrives later than other signals, so we want to remove it from the long path.
  wire depend_on_sb = |({rd_depend_on_sb, rs_depend_on_sb} & ~{rd_on_clear_combined, rs_on_clear_combined});
  wire any_score_dep = |{rd_depend_on_score_any, rs_depend_on_score_any};

  /* 原本的 dependency 組合邏輯 (註解保留)
  wire depend_on_score = |{rd_depend_on_score, rs_depend_on_score};
  assign dependency_o = depend_on_sb | (depend_on_score & score_i & allow_zero);
  */

  // 原本只扣除了 depend_on_sb 的清除路徑，現在我們統一處理
  assign dependency_o = depend_on_sb | any_score_dep; 

  // synopsys translate_off
  // always_ff @ (negedge clk_i) begin
  //   if (~reset_i) begin
  //     // 修改：檢查任何一個 score 與任何一個 clear 是否衝突
  //     assert((score_combined & clear_combined) == '0)
  //       else $error("[BSG_ERROR] score and clear on the same id cannot happen.");
  //   end
  // end
  // synopsys translate_on


endmodule

`BSG_ABSTRACT_MODULE(scoreboard)