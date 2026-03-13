`timescale 1ns/1ps
`include "bsg_vanilla_defines.svh"

import bsg_manycore_pkg::*;
import bsg_vanilla_pkg::*;

module testbench();

  // clock & reset generators
  logic clk_i;
  bsg_nonsynth_clock_gen #(.cycle_time_p(10)) clock_gen (.o(clk_i));

  logic network_reset_i;
  logic reset_i;
  bsg_nonsynth_reset_gen #(.reset_cycles_lo_p(4), .reset_cycles_hi_p(4)) reset_gen (
    .clk_i(clk_i),
    .async_reset_o(reset_i)
  );

  // Parameters for the cache under test
  // parameters must satisfy pc_width_lp+2 >= branch_pc_low_width_lp
  // and also >= jal_pc_low_width_lp.  Increase both tag and entries so
  // the PC width is wide enough for 20-bit JAL immediates.
  localparam icache_tag_width_p             = 12;
  localparam icache_entries_p               = 4096;
  localparam icache_block_size_in_words_p   = 4;   // four-word block
  localparam icache_addr_width_lp           = `BSG_SAFE_CLOG2(icache_entries_p/icache_block_size_in_words_p);
  localparam pc_width_lp                    = (icache_tag_width_p + `BSG_SAFE_CLOG2(icache_entries_p));
  localparam icache_block_offset_width_lp   = `BSG_SAFE_CLOG2(icache_block_size_in_words_p);

  // helper localparams used in prediction function (mirror those in the DUT)
  localparam branch_pc_low_width_lp = (RV32_Bimm_width_gp+1);
  localparam jal_pc_low_width_lp    = (RV32_Jimm_width_gp+1);
  localparam branch_pc_high_width_lp = (pc_width_lp+2) - branch_pc_low_width_lp;
  localparam jal_pc_high_width_lp    = (pc_width_lp+2) - jal_pc_low_width_lp;

  // interface signals
  logic v_i, w_i, flush_i, read_pc_plus4_i;
  logic [pc_width_lp-1:0]              w_pc_i;
  logic [RV32_instr_width_gp-1:0]      w_instr_i;
  logic [pc_width_lp-1:0]              pc_i;
  logic [pc_width_lp-1:0]              jalr_prediction_i;

  logic [RV32_instr_width_gp-1:0]      instr_o, instr1_o;
  logic [pc_width_lp-1:0]               pred_or_jump_addr_o, pred_or_jump_addr1_o;
  logic                                branch_predicted_taken_o, branch_predicted_taken1_o;
  logic                                is_block_boundary_o;
  logic [pc_width_lp-1:0]              pc_r_o;
  logic                                icache_miss_o;
  logic                                icache_flush_r_o;

  // DUT instantiation
  icache #(
    .icache_tag_width_p(icache_tag_width_p),
    .icache_entries_p(icache_entries_p),
    .icache_block_size_in_words_p(icache_block_size_in_words_p)
  ) dut (.*);

  // storage for written data and expected predictions
  logic [RV32_instr_width_gp-1:0] saved_instr [0:icache_block_size_in_words_p-1];
  logic [pc_width_lp-1:0]         saved_pred  [0:icache_block_size_in_words_p-1];

  // helper to compute predicted address exactly like the icache logic
  function automatic [pc_width_lp-1:0] calc_pred;
    input [pc_width_lp-1:0]              pc;
    input [RV32_instr_width_gp-1:0]      instr_bits;

    // convert to instruction struct so we can reference fields
    instruction_s instr;

    // reuse many of the localparams from above
    logic [branch_pc_low_width_lp-1:0] branch_imm_val;
    logic [branch_pc_low_width_lp-1:0] branch_pc_val;
    logic [jal_pc_low_width_lp-1:0]    jal_imm_val;
    logic [jal_pc_low_width_lp-1:0]    jal_pc_val;
    logic [branch_pc_low_width_lp-1:0] branch_pc_lower_res;
    logic                              branch_pc_lower_cout;
    logic [jal_pc_low_width_lp-1:0]    jal_pc_lower_res;
    logic                              jal_pc_lower_cout;

    logic imm_sign;
    logic pc_lower_cout;
    logic sel_pc, sel_pc_p1;

    logic [branch_pc_high_width_lp-1:0] branch_pc_high;
    logic [jal_pc_high_width_lp-1:0]    jal_pc_high;
    logic [branch_pc_high_width_lp-1:0] branch_pc_high_out;
    logic [jal_pc_high_width_lp-1:0]    jal_pc_high_out;
    logic [pc_width_lp+2-1:0]           branch_pc_full;
    logic [pc_width_lp+2-1:0]           jal_pc_full;

    begin
      // perform the bit-vector -> struct copy
      instr = instr_bits;

      branch_imm_val = `RV32_Bimm_13extract(instr_bits);
      branch_pc_val  = branch_pc_low_width_lp'({pc,2'b0});
      jal_imm_val    = `RV32_Jimm_21extract(instr_bits);
      jal_pc_val     = jal_pc_low_width_lp'({pc,2'b0});

      {branch_pc_lower_cout, branch_pc_lower_res} = {1'b0, branch_imm_val} + {1'b0, branch_pc_val};
      {jal_pc_lower_cout,    jal_pc_lower_res   } = {1'b0, jal_imm_val}    + {1'b0, jal_pc_val   };

      imm_sign    = (instr.op == `RV32_BRANCH) ? branch_imm_val[RV32_Bimm_width_gp]
                                                : jal_imm_val[RV32_Jimm_width_gp];
      pc_lower_cout = (instr.op == `RV32_BRANCH) ? branch_pc_lower_cout
                                                 : jal_pc_lower_cout;

      sel_pc   = ~(imm_sign ^ pc_lower_cout);
      sel_pc_p1 = (~imm_sign) & pc_lower_cout;

      branch_pc_high = pc[(branch_pc_low_width_lp-2)+:branch_pc_high_width_lp];
      jal_pc_high    = pc[(jal_pc_low_width_lp-2)+:jal_pc_high_width_lp];

      if (sel_pc) begin
        branch_pc_high_out = branch_pc_high;
        jal_pc_high_out    = jal_pc_high;
      end
      else if (sel_pc_p1) begin
        branch_pc_high_out = branch_pc_high + 1'b1;
        jal_pc_high_out    = jal_pc_high    + 1'b1;
      end
      else begin
        branch_pc_high_out = branch_pc_high - 1'b1;
        jal_pc_high_out    = jal_pc_high    - 1'b1;
      end

      branch_pc_full = {branch_pc_high_out, `RV32_Bimm_13extract(instr_bits)};
      jal_pc_full    = {jal_pc_high_out,    `RV32_Jimm_21extract(instr_bits)};

      if (instr.op == `RV32_JAL_OP)
        calc_pred = jal_pc_full[2+:pc_width_lp];
      else if (instr.op == `RV32_JALR_OP)
        // in the absence of a real jalr prediction we assume 0
        calc_pred = '0;
      else
        // default to branch_pc for non-jump instructions (matches DUT logic)
        calc_pred = branch_pc_full[2+:pc_width_lp];
    end
  endfunction

  // stimulus
  initial begin
    // defaults
    // clear any internal write counter by pulsing network reset
    network_reset_i = 1'b1;
    @(posedge clk_i);
    network_reset_i = 1'b0;

    // defaults (use sized literals to avoid lint warnings)
    v_i = 1'b0; w_i = 1'b0; flush_i = 1'b0; read_pc_plus4_i = 1'b1;
    @(negedge reset_i);

    // --- BLOCK WRITE -------------------------------------------------------
    for (int idx = 0; idx < icache_block_size_in_words_p; idx++) begin
      // local vars for injection computation must be declared at block start
      instruction_s w_instr_s;
      logic write_branch_instr;
      logic write_jal_instr;
      logic [branch_pc_low_width_lp-1:0] branch_imm_val;
      logic [jal_pc_low_width_lp-1:0]    jal_imm_val;
      logic [branch_pc_low_width_lp-1:0] branch_pc_val;
      logic [jal_pc_low_width_lp-1:0]    jal_pc_val;
      logic branch_pc_lower_cout, jal_pc_lower_cout;
      logic [branch_pc_low_width_lp-1:0] branch_pc_lower_res;
      logic [jal_pc_low_width_lp-1:0]    jal_pc_lower_res;
      logic [RV32_instr_width_gp-1:0]    injected_instr;

      w_pc_i = pc_width_lp'(idx);            // tag=0, addr=0, offset=idx
      if (idx == 2) begin
        // use a simple JAL with zero offset (opcode 0x6F)
        w_instr_i = 32'h0000_006F;
      end else begin
        w_instr_i = 32'h1000_0000 + idx;
      end
      // determine what actually gets written into icache (injection logic)
      w_instr_s = w_instr_i;
      write_branch_instr = w_instr_s.op ==? `RV32_BRANCH;
      write_jal_instr    = w_instr_s.op ==? `RV32_JAL_OP;
      branch_imm_val     = `RV32_Bimm_13extract(w_instr_s);
      jal_imm_val        = `RV32_Jimm_21extract(w_instr_s);
      branch_pc_val      = branch_pc_low_width_lp'({w_pc_i,2'b0});
      jal_pc_val         = jal_pc_low_width_lp'({w_pc_i,2'b0});
      {branch_pc_lower_cout, branch_pc_lower_res} = {1'b0, branch_imm_val} + {1'b0, branch_pc_val};
      {jal_pc_lower_cout,    jal_pc_lower_res   } = {1'b0, jal_imm_val}    + {1'b0, jal_pc_val   };
      injected_instr = write_branch_instr
        ? `RV32_Bimm_12inject1(w_instr_s, branch_pc_lower_res)
        : (write_jal_instr
          ? `RV32_Jimm_20inject1(w_instr_s, jal_pc_lower_res)
          : w_instr_s);
      saved_instr[idx] = injected_instr;
      // prediction should be based on the cached copy (after injection)
      saved_pred[idx]  = calc_pred(w_pc_i, injected_instr);

      v_i = 1'b1; w_i = 1'b1;
      @(posedge clk_i);
    end
    v_i = 0; w_i = 0;

    // give a couple cycles for the writes to propagate through memory
    @(posedge clk_i);
    @(posedge clk_i);

    // disable the energy-saving heuristic so we actually read all words
    read_pc_plus4_i = 1'b0;

    // --- SEQUENTIAL READ & DUAL-ISSUE ------------------------------
    for (int idx = 0; idx < icache_block_size_in_words_p; idx++) begin
      pc_i = idx;
      v_i  = 1'b1; w_i = 1'b0;
@(posedge clk_i);
      @(posedge clk_i); // allow outputs to settle

      $display("DEBUG: idx=%0d instr_o=%h instr1_o=%h saved=%h next_saved=%h", idx, instr_o, instr1_o, saved_instr[idx], (idx < icache_block_size_in_words_p-1) ? saved_instr[idx+1] : 'hX);
      if (instr_o !== saved_instr[idx]) begin
        $display("ERROR: instr mismatch offset %0d (got %h expected %h)", idx, instr_o, saved_instr[idx]);
        $finish;
      end

      if (idx < icache_block_size_in_words_p-1) begin
        if (instr1_o !== saved_instr[idx+1]) begin
          $display("ERROR: instr1 mismatch at offset %0d", idx);
          $finish;
        end
      end

      // boundary check
      if (idx == icache_block_size_in_words_p-1) begin
        if (is_block_boundary_o !== 1'b1) begin
          $display("ERROR: boundary flag not set at last offset");
          $finish;
        end
      end

      // branch prediction for primary read
      if (pred_or_jump_addr_o !== saved_pred[idx]) begin
        $display("ERROR: prediction mismatch (got %0h expected %0h) at idx %0d",
                 pred_or_jump_addr_o, saved_pred[idx], idx);
        $finish;
      end
      // branch prediction for secondary (instr1) when valid
      if (idx < icache_block_size_in_words_p-1) begin
        if (pred_or_jump_addr1_o !== saved_pred[idx+1]) begin
          $display("ERROR: prediction1 mismatch (got %0h expected %0h) at idx %0d",
                   pred_or_jump_addr1_o, saved_pred[idx+1], idx);
          $finish;
        end
      end
    end

    $display("icache_tb PASSED");
    $finish;
  end

endmodule
