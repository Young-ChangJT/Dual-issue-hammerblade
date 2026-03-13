`include "bsg_vanilla_defines.svh"

module vanilla_issue_ctrl 
  import bsg_vanilla_pkg::*;
  (
    // [新增] 傳入原始指令，用來萃取暫存器編號 (rs1, rs2, rd)
    input [31:0] instr0_i
    , input [31:0] instr1_i
    // 來自兩個解碼器的控制訊號
    , input decode_s decode0_i
    , input fp_decode_s fp_decode0_i
    , input decode_s decode1_i
    , input fp_decode_s fp_decode1_i

    // 來自 Scoreboard 的依賴訊號 (使用你修改後的雙埠版本)
    , input int_sb_dep_i
    , input float_sb_dep_i

    // 其他限制條件
    , input icache_block_boundary_i // 如果 Instr A 是 Block 的最後一條則為 1

    // 輸出判斷結果
    , output logic can_issue_dual_o
  );

  // 1. 類別判定 (Category Logic)
  // 檢查是否為「一浮一整/記憶體」的標準組合
  wire is_pair_fp_int = (decode0_i.is_fp_op && !decode1_i.is_fp_op);
  wire is_pair_int_fp = (!decode0_i.is_fp_op && decode1_i.is_fp_op);

  // 2. 專案要求：支援 FP Load 與 FPU Compute 並行
  // 即使 FP Load 使用浮點暫存器，我們也希望將其視為整數/記憶體類別以利配對
  wire instr0_is_fp_load = decode0_i.is_load_op && decode0_i.write_frd;
  wire instr1_is_fp_load = decode1_i.is_load_op && decode1_i.write_frd;
  
  wire is_special_fp_load_pair = (instr0_is_fp_load && fp_decode1_i.is_fpu_float_op) ||
                                 (fp_decode0_i.is_fpu_float_op && instr1_is_fp_load);

  // 3. 分支與跳轉限制 (Control Hazard)
  // [修正] 只要任一條指令是分支、跳轉或返回，就絕對不能雙發射
  wire instr0_is_control = decode0_i.is_branch_op  || decode0_i.is_jal_op    || 
                         decode0_i.is_jalr_op     || decode0_i.is_mret_op   ||
                         decode0_i.is_barsend_op  || decode0_i.is_barrecv_op||
                         decode0_i.is_fence_op    || decode0_i.is_idiv_op;

  wire instr1_is_control = decode1_i.is_branch_op  || decode1_i.is_jal_op    || 
                         decode1_i.is_jalr_op     || decode1_i.is_mret_op   ||
                         decode1_i.is_barsend_op  || decode1_i.is_barrecv_op||
                         decode1_i.is_fence_op    || decode1_i.is_idiv_op;

  wire any_is_control    = instr0_is_control || instr1_is_control;

  // 4. 結構與依賴限制
  // - 不能跨越 128-bit I-Cache Block 邊界
  // - 不能有 Scoreboard 偵測到的長延遲依賴 (如 IDIV, FDIV, Remote Load) [cite: 28, 36, 37]
  wire no_structural_hazards = !icache_block_boundary_i && !int_sb_dep_i && !float_sb_dep_i;

  // 5. [新增] 內部數據冒險檢查 (Intra-pair RAW Hazard)
  // 檢查 instr1 是否需要讀取 instr0 即將寫入的暫存器
  
  wire [4:0] instr0_rd  = instr0_i[11:7];
  // 從原始指令中萃取出 rs1 (位元 19 到 15)
  wire [4:0] instr0_rs1 = instr0_i[19:15];
  wire [4:0] instr1_rs1 = instr1_i[19:15];
  wire [4:0] instr1_rs2 = instr1_i[24:20];
  wire [4:0] instr1_rs3 = instr1_i[31:27]; // FMA/FP 指令會用到 rs3

  // (A) 整數暫存器相依檢查 (注意：x0 是 Hardwired to 0，所以 rd=0 不算 hazard)
  wire instr0_writes_int = decode0_i.write_rd && (instr0_rd != 5'd0);
  wire instr1_reads_instr0_int = instr0_writes_int && (
      (decode1_i.read_rs1 && (instr1_rs1 == instr0_rd)) ||
      (decode1_i.read_rs2 && (instr1_rs2 == instr0_rd))
  );

  // (B) 浮點暫存器相依檢查 (浮點沒有 f0=0 的限制，所以直接比對)
  wire instr0_writes_fp = decode0_i.write_frd;
  wire instr1_reads_instr0_fp = instr0_writes_fp && (
      (decode1_i.read_frs1 && (instr1_rs1 == instr0_rd)) ||
      (decode1_i.read_frs2 && (instr1_rs2 == instr0_rd)) ||
      (decode1_i.read_frs3 && (instr1_rs3 == instr0_rd))
  );

  // 總結內部冒險
  wire intra_pair_hazard = instr1_reads_instr0_int || instr1_reads_instr0_fp;

  // 6. [新增] 暫存器埠結構衝突 (Structural Port Hazards)
  // 如果遇到需要跨界讀寫暫存器的指令，強制降級為單發射，避免搶奪有限的暫存器讀寫埠
  // wire hazard_int_read_port = decode1_i.read_rs1; // FP instr reading INT
  wire hazard_int_read_port = decode1_i.read_rs1 && decode0_i.read_rs1 && (instr1_rs1 != instr0_rs1);
  wire hazard_fp_read_port = decode0_i.read_frs1 | decode0_i.read_frs2 | decode0_i.read_frs3; // INT instr reading FP
  wire hazard_int_write_port = decode1_i.write_rd; // FP instr writing INT
  
  wire struct_port_hazards = hazard_int_read_port | hazard_fp_read_port | hazard_int_write_port;
  // 最終發射判定
  always_comb begin
    // [修改] 加入 !intra_pair_hazard 作為雙發射的必要條件
    if (no_structural_hazards && !any_is_control && !intra_pair_hazard && !struct_port_hazards) begin
        can_issue_dual_o = is_pair_fp_int || is_pair_int_fp || is_special_fp_load_pair;
    end else begin
        can_issue_dual_o = 1'b0;
    end
  end

endmodule