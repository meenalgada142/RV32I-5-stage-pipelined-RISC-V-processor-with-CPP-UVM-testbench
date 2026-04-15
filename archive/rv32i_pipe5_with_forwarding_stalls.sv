//============================================================================
// rv32i_pipe5_with_forwarding_stalls.sv
// 5-stage pipeline with forwarding AND stall logic for load-use hazards
//============================================================================

module rv32i_pipe5_with_forwarding_stalls (
    input  logic        clk,
    input  logic        rst,
    output logic [31:0] alu_result,
    output logic        zero
);

    //========================================================================
    // Memory
    //========================================================================
    logic [31:0] instr_mem [0:255];
    logic [31:0] data_mem [0:255];

    //========================================================================
    // IF Stage with Stall Control
    //========================================================================
    logic [31:0] pc, pc_next, pc_plus4;
    logic [31:0] if_instr;
    logic        pc_write_enable;
    logic        if_id_write_enable;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            pc <= 32'd0;
        end else if (pc_write_enable) begin
            pc <= pc_next;
        end
        // else: PC stays the same (stalled)
    end

    assign pc_plus4 = pc + 32'd4;
    assign if_instr = instr_mem[pc[9:2]];
    assign pc_next  = pc_plus4;

    //========================================================================
    // IF/ID Pipeline Register with Write Enable
    //========================================================================
    logic [31:0] if_id_pc_plus4;
    logic [31:0] if_id_instr;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            if_id_pc_plus4 <= 32'd0;
            if_id_instr    <= 32'd0;
        end else if (if_id_write_enable) begin
            if_id_pc_plus4 <= pc_plus4;
            if_id_instr    <= if_instr;
        end
        // else: IF/ID stays the same (stalled)
    end

    //========================================================================
    // ID Stage
    //========================================================================
    logic [4:0]  id_rs1, id_rs2, id_rd;
    logic [31:0] id_imm;
    logic [3:0]  id_alu_op;
    logic        id_alu_src, id_mem_read, id_mem_write, id_mem_to_reg;
    logic        id_reg_write, id_branch, id_jump;
    logic [31:0] id_rs1_data, id_rs2_data;

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
    // ID/EX Pipeline Register with Bubble Insertion
    //========================================================================
    logic [31:0] id_ex_pc_plus4;
    logic [4:0]  id_ex_rs1, id_ex_rs2, id_ex_rd;
    logic [31:0] id_ex_rs1_data, id_ex_rs2_data, id_ex_imm;
    logic [3:0]  id_ex_alu_op;
    logic        id_ex_alu_src, id_ex_mem_read, id_ex_mem_write;
    logic        id_ex_mem_to_reg, id_ex_reg_write, id_ex_branch, id_ex_jump;
    logic        insert_bubble;

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
        end else if (insert_bubble) begin
            // Insert bubble (NOP) - zero all control signals
            id_ex_pc_plus4    <= id_ex_pc_plus4;  // Keep PC for reference
            id_ex_rs1         <= 5'd0;
            id_ex_rs2         <= 5'd0;
            id_ex_rd          <= 5'd0;
            id_ex_rs1_data    <= 32'd0;
            id_ex_rs2_data    <= 32'd0;
            id_ex_imm         <= 32'd0;
            id_ex_alu_op      <= 4'd0;
            id_ex_alu_src     <= 1'b0;
            id_ex_mem_read    <= 1'b0;  // ← Critical: disable load
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
    // Hazard Detection Unit
    //========================================================================
    rv32i_hazard_detection hz_detect (
        .id_rs1            (id_rs1),
        .id_rs2            (id_rs2),
        .ex_rd             (id_ex_rd),
        .ex_memread        (id_ex_mem_read),
        .stall             (),  // Used as control
        .pc_write_enable   (pc_write_enable),
        .if_id_write_enable(if_id_write_enable),
        .insert_bubble     (insert_bubble)
    );

    //========================================================================
    // EX Stage with Forwarding
    //========================================================================
    logic [31:0] ex_alu_in1, ex_alu_in2;
    logic [31:0] ex_alu_result;
    logic        ex_zero;
    logic [1:0]  forward_a, forward_b;
    logic [31:0] forwarded_rs2_data;  // Store data path, separate from ALU B

    rv32i_forwarding_unit ex_forwarding (
        .ex_rs1       (id_ex_rs1),
        .ex_rs2       (id_ex_rs2),
        .mem_rd       (ex_mem_rd),
        .mem_regwrite (ex_mem_reg_write),
        .wb_rd        (mem_wb_rd),
        .wb_regwrite  (mem_wb_reg_write),
        .forward_a    (forward_a),
        .forward_b    (forward_b)
    );

    always_comb begin
        case (forward_a)
            2'b00: ex_alu_in1 = id_ex_rs1_data;
            2'b01: ex_alu_in1 = wb_write_data;
            2'b10: ex_alu_in1 = ex_mem_alu_result;
            default: ex_alu_in1 = id_ex_rs1_data;
        endcase
    end

    // ALU input B mux.
    // When alu_src=1 the ALU needs the immediate; guard non-zero forward_b cases
    // so they cannot corrupt the immediate with rs2 forwarded data.
    always_comb begin
        case (forward_b)
            2'b00: begin
                if (id_ex_alu_src)
                    ex_alu_in2 = id_ex_imm;
                else
                    ex_alu_in2 = id_ex_rs2_data;
            end
            2'b01: ex_alu_in2 = id_ex_alu_src ? id_ex_imm : wb_write_data;
            2'b10: ex_alu_in2 = id_ex_alu_src ? id_ex_imm : ex_mem_alu_result;
            default: ex_alu_in2 = id_ex_rs2_data;
        endcase
    end

    // Store data forwarding mux.
    // Uses the same forward_b select lines but drives ex_mem_rs2_data (store data),
    // keeping it independent of the ALU address path.
    always_comb begin
        case (forward_b)
            2'b10:   forwarded_rs2_data = ex_mem_alu_result; // MEM stage (highest priority)
            2'b01:   forwarded_rs2_data = wb_write_data;     // WB stage
            default: forwarded_rs2_data = id_ex_rs2_data;    // Register file (no hazard)
        endcase
    end

    rv32i_alu ex_alu (
        .a      (ex_alu_in1),
        .b      (ex_alu_in2),
        .alu_op (id_ex_alu_op),
        .result (ex_alu_result),
        .zero   (ex_zero)
    );

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
            ex_mem_rs2_data    <= forwarded_rs2_data;
            ex_mem_rd          <= id_ex_rd;
            ex_mem_mem_read    <= id_ex_mem_read;
            ex_mem_mem_write   <= id_ex_mem_write;
            ex_mem_mem_to_reg  <= id_ex_mem_to_reg;
            ex_mem_reg_write   <= id_ex_reg_write;
            ex_mem_branch      <= id_ex_branch;
            ex_mem_take_branch <= (id_ex_branch && ex_zero);
        end
    end

    //========================================================================
    // MEM Stage
    //========================================================================
    logic [31:0] mem_dmem_rdata;

    assign mem_dmem_rdata = (ex_mem_mem_read) ? data_mem[ex_mem_alu_result[9:2]] : 32'd0;

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
    // WB Stage
    //========================================================================
    logic [31:0] wb_write_data;

    assign wb_write_data = (mem_wb_mem_to_reg) ? mem_wb_dmem_data : mem_wb_alu_result;

    //========================================================================
    // Debug / Output
    //========================================================================
    assign alu_result = ex_mem_alu_result;
    assign zero       = ex_zero;

endmodule
