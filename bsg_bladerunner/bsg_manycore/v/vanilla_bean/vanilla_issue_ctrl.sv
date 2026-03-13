`include "bsg_vanilla_defines.svh"

module vanilla_issue_ctrl 
  import bsg_vanilla_pkg::*;
  (
    input [31:0] instr0_i
    , input [31:0] instr1_i
    , input decode_s decode0_i
    , input fp_decode_s fp_decode0_i
    , input decode_s decode1_i
    , input fp_decode_s fp_decode1_i
    , input int_sb_dep_i
    , input float_sb_dep_i
    , input icache_block_boundary_i 
    , output logic can_issue_dual_o
  );


  wire is_pair_fp_int = (decode0_i.is_fp_op && !decode1_i.is_fp_op);
  wire is_pair_int_fp = (!decode0_i.is_fp_op && decode1_i.is_fp_op);
  wire instr0_is_fp_load = decode0_i.is_load_op && decode0_i.write_frd;
  wire instr1_is_fp_load = decode1_i.is_load_op && decode1_i.write_frd;
  
  wire is_special_fp_load_pair = (instr0_is_fp_load && fp_decode1_i.is_fpu_float_op) ||
                                 (fp_decode0_i.is_fpu_float_op && instr1_is_fp_load);

  wire instr0_is_control = decode0_i.is_branch_op  || decode0_i.is_jal_op    || 
                         decode0_i.is_jalr_op     || decode0_i.is_mret_op   ||
                         decode0_i.is_barsend_op  || decode0_i.is_barrecv_op||
                         decode0_i.is_fence_op    || decode0_i.is_idiv_op;

  wire instr1_is_control = decode1_i.is_branch_op  || decode1_i.is_jal_op    || 
                         decode1_i.is_jalr_op     || decode1_i.is_mret_op   ||
                         decode1_i.is_barsend_op  || decode1_i.is_barrecv_op||
                         decode1_i.is_fence_op    || decode1_i.is_idiv_op;

  wire any_is_control    = instr0_is_control || instr1_is_control;
  wire no_structural_hazards = !icache_block_boundary_i && !int_sb_dep_i && !float_sb_dep_i;  
  wire [4:0] instr0_rd  = instr0_i[11:7];
  wire [4:0] instr0_rs1 = instr0_i[19:15];
  wire [4:0] instr1_rs1 = instr1_i[19:15];
  wire [4:0] instr1_rs2 = instr1_i[24:20];
  wire [4:0] instr1_rs3 = instr1_i[31:27];

  wire instr0_writes_int = decode0_i.write_rd && (instr0_rd != 5'd0);
  wire instr1_reads_instr0_int = instr0_writes_int && (
      (decode1_i.read_rs1 && (instr1_rs1 == instr0_rd)) ||
      (decode1_i.read_rs2 && (instr1_rs2 == instr0_rd))
  );

  wire instr0_writes_fp = decode0_i.write_frd;
  wire instr1_reads_instr0_fp = instr0_writes_fp && (
      (decode1_i.read_frs1 && (instr1_rs1 == instr0_rd)) ||
      (decode1_i.read_frs2 && (instr1_rs2 == instr0_rd)) ||
      (decode1_i.read_frs3 && (instr1_rs3 == instr0_rd))
  );

  wire intra_pair_hazard = instr1_reads_instr0_int || instr1_reads_instr0_fp;
  wire hazard_int_read_port = decode1_i.read_rs1 && decode0_i.read_rs1 && (instr1_rs1 != instr0_rs1);
  wire hazard_fp_read_port = decode0_i.read_frs1 | decode0_i.read_frs2 | decode0_i.read_frs3;
  wire hazard_int_write_port = decode1_i.write_rd; 
  
  wire struct_port_hazards = hazard_int_read_port | hazard_fp_read_port | hazard_int_write_port;
  always_comb begin
    if (no_structural_hazards && !any_is_control && !intra_pair_hazard && !struct_port_hazards) begin
        can_issue_dual_o = is_pair_fp_int || is_pair_int_fp || is_special_fp_load_pair;
    end else begin
        can_issue_dual_o = 1'b0;
    end
  end

endmodule