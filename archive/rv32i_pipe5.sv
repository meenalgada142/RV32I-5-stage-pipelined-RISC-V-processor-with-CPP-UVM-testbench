//============================================================================
// rv32i_pipe5.sv
// 5-stage pipelined RISC-V (RV32I) datapath
// Stages: IF -> ID -> EX -> MEM -> WB
//============================================================================

module rv32i_pipe5 (
    input  logic        clk,
    input  logic        rst,
    output logic [31:0] alu_result,
    output logic        zero
);

    //========================================================================
    // Memory (instruction & data)
    //========================================================================
    logic [31:0] instr_mem [0:255];  // 256 x 32-bit instruction memory
    logic [31:0] data_mem [0:255];   // 256 x 32-bit data memory

    //========================================================================
    // IF Stage: Instruction Fetch
    //========================================================================
    logic [31:0] pc, pc_next, pc_plus4;
    logic [31:0] if_instr;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            pc <= 32'd0;
        end else begin
            pc <= pc_next;
        end
    end

    assign pc_plus4 = pc + 32'd4;
    assign if_instr = instr_mem[pc[9:2]];  // 256 instructions max
    assign pc_next  = pc_plus4;  // no branch yet (stubbed)

    //========================================================================
    // IF/ID Pipeline Register
    //========================================================================
    logic [31:0] if_id_pc_plus4;
    logic [31:0] if_id_instr;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            if_id_pc_plus4 <= 32'd0;
            if_id_instr    <= 32'd0;
        end else begin
            if_id_pc_plus4 <= pc_plus4;
            if_id_instr    <= if_instr;
        end
    end

    //========================================================================
    // ID Stage: Instruction Decode & Register Read
    //========================================================================
    logic [4:0]  id_rs1, id_rs2, id_rd;
    logic [31:0] id_imm;
    logic [3:0]  id_alu_op;
    logic        id_alu_src, id_mem_read, id_mem_write, id_mem_to_reg;
    logic        id_reg_write, id_branch, id_jump;
    logic [31:0] id_rs1_data, id_rs2_data;

    // Decoder instance
    rv32i_decoder id_decoder (
        .instr      (if_id_instr),
        .rs1        (id_rs1),
        .rs2        (id_rs2),
        .rd         (id_rd),
        .imm        (id_imm),
        .alu_op     (id_alu_op),
        .reg_write  (id_reg_write),
        .mem_read   (id_mem_read),
        .mem_write  (id_mem_write),
        .mem_to_reg (id_mem_to_reg),
        .alu_src    (id_alu_src),
        .branch     (id_branch),
        .jump       (id_jump)
    );

    // Register file instance (writeback from WB stage)
    rv32i_regfile id_regfile (
        .clk         (clk),
        .rst         (rst),
        .rs1         (id_rs1),
        .rs2         (id_rs2),
        .rd          (mem_wb_rd),
        .wd          (wb_write_data),
        .write_enable(mem_wb_reg_write),
        .rd1         (id_rs1_data),
        .rd2         (id_rs2_data)
    );

    //========================================================================
    // ID/EX Pipeline Register
    //========================================================================
    logic [31:0] id_ex_pc_plus4;
    logic [4:0]  id_ex_rs1, id_ex_rs2, id_ex_rd;
    logic [31:0] id_ex_rs1_data, id_ex_rs2_data, id_ex_imm;
    logic [3:0]  id_ex_alu_op;
    logic        id_ex_alu_src, id_ex_mem_read, id_ex_mem_write;
    logic        id_ex_mem_to_reg, id_ex_reg_write, id_ex_branch, id_ex_jump;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            id_ex_pc_plus4    <= 32'd0;
            id_ex_rs1         <= 5'd0;
            id_ex_rs2         <= 5'd0;
            id_ex_rd          <= 5'd0;
            id_ex_rs1_data    <= 32'd0;
            id_ex_rs2_data    <= 32'd0;
            id_ex_imm         <= 32'd0;
            id_ex_alu_op      <= 4'd0;
            id_ex_alu_src     <= 1'b0;
            id_ex_mem_read    <= 1'b0;
            id_ex_mem_write   <= 1'b0;
            id_ex_mem_to_reg  <= 1'b0;
            id_ex_reg_write   <= 1'b0;
            id_ex_branch      <= 1'b0;
            id_ex_jump        <= 1'b0;
        end else begin
            id_ex_pc_plus4    <= if_id_pc_plus4;
            id_ex_rs1         <= id_rs1;
            id_ex_rs2         <= id_rs2;
            id_ex_rd          <= id_rd;
            id_ex_rs1_data    <= id_rs1_data;
            id_ex_rs2_data    <= id_rs2_data;
            id_ex_imm         <= id_imm;
            id_ex_alu_op      <= id_alu_op;
            id_ex_alu_src     <= id_alu_src;
            id_ex_mem_read    <= id_mem_read;
            id_ex_mem_write   <= id_mem_write;
            id_ex_mem_to_reg  <= id_mem_to_reg;
            id_ex_reg_write   <= id_reg_write;
            id_ex_branch      <= id_branch;
            id_ex_jump        <= id_jump;
        end
    end

    //========================================================================
    // EX Stage: Execute (ALU + Branch Decision)
    //========================================================================
    logic [31:0] ex_alu_in1, ex_alu_in2;
    logic [31:0] ex_alu_result;
    logic        ex_zero;
    logic [31:0] ex_branch_target;
    logic        ex_take_branch;

    assign ex_alu_in1 = id_ex_rs1_data;
    assign ex_alu_in2 = (id_ex_alu_src) ? id_ex_imm : id_ex_rs2_data;

    // ALU instance
    rv32i_alu ex_alu (
        .a      (ex_alu_in1),
        .b      (ex_alu_in2),
        .alu_op (id_ex_alu_op),
        .result (ex_alu_result),
        .zero   (ex_zero)
    );

    assign ex_branch_target = id_ex_pc_plus4 + id_ex_imm;
    assign ex_take_branch   = id_ex_branch && ex_zero;  // BEQ example (simplified)

    //========================================================================
    // EX/MEM Pipeline Register
    //========================================================================
    logic [31:0] ex_mem_pc_plus4;
    logic [31:0] ex_mem_alu_result;
    logic [31:0] ex_mem_rs2_data;
    logic [4:0]  ex_mem_rd;
    logic        ex_mem_mem_read, ex_mem_mem_write;
    logic        ex_mem_mem_to_reg, ex_mem_reg_write;
    logic        ex_mem_branch, ex_mem_take_branch;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            ex_mem_pc_plus4    <= 32'd0;
            ex_mem_alu_result  <= 32'd0;
            ex_mem_rs2_data    <= 32'd0;
            ex_mem_rd          <= 5'd0;
            ex_mem_mem_read    <= 1'b0;
            ex_mem_mem_write   <= 1'b0;
            ex_mem_mem_to_reg  <= 1'b0;
            ex_mem_reg_write   <= 1'b0;
            ex_mem_branch      <= 1'b0;
            ex_mem_take_branch <= 1'b0;
        end else begin
            ex_mem_pc_plus4    <= id_ex_pc_plus4;
            ex_mem_alu_result  <= ex_alu_result;
            ex_mem_rs2_data    <= id_ex_rs2_data;
            ex_mem_rd          <= id_ex_rd;
            ex_mem_mem_read    <= id_ex_mem_read;
            ex_mem_mem_write   <= id_ex_mem_write;
            ex_mem_mem_to_reg  <= id_ex_mem_to_reg;
            ex_mem_reg_write   <= id_ex_reg_write;
            ex_mem_branch      <= id_ex_branch;
            ex_mem_take_branch <= ex_take_branch;
        end
    end

    //========================================================================
    // MEM Stage: Memory Access (data memory read/write)
    //========================================================================
    logic [31:0] mem_dmem_rdata;

    // Memory read
    assign mem_dmem_rdata = (ex_mem_mem_read) ? data_mem[ex_mem_alu_result[9:2]] : 32'd0;

    // Memory write (on clock edge)
    always_ff @(posedge clk) begin
        if (ex_mem_mem_write) begin
            data_mem[ex_mem_alu_result[9:2]] <= ex_mem_rs2_data;
        end
    end

    //========================================================================
    // MEM/WB Pipeline Register
    //========================================================================
    logic [31:0] mem_wb_alu_result;
    logic [31:0] mem_wb_dmem_data;
    logic [4:0]  mem_wb_rd;
    logic        mem_wb_mem_to_reg, mem_wb_reg_write;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            mem_wb_alu_result  <= 32'd0;
            mem_wb_dmem_data   <= 32'd0;
            mem_wb_rd          <= 5'd0;
            mem_wb_mem_to_reg  <= 1'b0;
            mem_wb_reg_write   <= 1'b0;
        end else begin
            mem_wb_alu_result  <= ex_mem_alu_result;
            mem_wb_dmem_data   <= mem_dmem_rdata;
            mem_wb_rd          <= ex_mem_rd;
            mem_wb_mem_to_reg  <= ex_mem_mem_to_reg;
            mem_wb_reg_write   <= ex_mem_reg_write;
        end
    end

    //========================================================================
    // WB Stage: Writeback
    //========================================================================
    logic [31:0] wb_write_data;

    assign wb_write_data = (mem_wb_mem_to_reg) ? mem_wb_dmem_data : mem_wb_alu_result;

    // Register file writeback is handled in ID stage (see regfile instantiation)

    //========================================================================
    // Debug / Output
    //========================================================================
    assign alu_result = ex_mem_alu_result;
    assign zero       = ex_zero;

endmodule
