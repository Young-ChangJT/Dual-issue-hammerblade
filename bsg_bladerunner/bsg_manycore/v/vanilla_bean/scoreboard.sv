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
    , parameter num_score_port_p = 2 
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
      score_combined[i] = |score_by_port_t[i];
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

  wire depend_on_sb = |({rd_depend_on_sb, rs_depend_on_sb} & ~{rd_on_clear_combined, rs_on_clear_combined});
  wire any_score_dep = |{rd_depend_on_score_any, rs_depend_on_score_any};

  assign dependency_o = depend_on_sb | any_score_dep; 


endmodule

`BSG_ABSTRACT_MODULE(scoreboard)