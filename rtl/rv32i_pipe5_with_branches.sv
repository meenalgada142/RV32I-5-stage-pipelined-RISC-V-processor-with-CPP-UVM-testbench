////////////////////////////////////////////////////////////////////////////////
//  rv32i_pipe5_with_branches.sv
//  
//  5-Stage Pipeline with Branch Support and Control Hazard Handling
//  
//  Supports: Forwarding, Stalls (load-use), and Branch flush
//  Branch instructions: BEQ, BNE (with correct PC redirection and flush)
//
//============================================================================

module rv32i_pipe5_with_branches (
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
    // IF Stage with Branch Control and Stall Control
    //========================================================================
    logic [31:0] pc, pc_next, pc_plus4;
    logic [31:0] if_instr;
    logic        pc_write_enable;
    logic        if_id_write_enable;
    logic        flush_if_id;        // Branch flush signal
    logic        branch_taken;       // From branch control
    logic [31:0] branch_pc_next;     // From branch control
    
    // Jump signals
    logic        jump_taken;         // From EX stage
    logic [31:0] jump_target;        // From EX stage
    
    // Intermediate flush signals (before combining)
    logic        br_flush_if_id, br_flush_id_ex;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            pc <= 32'd0;
        end else if (pc_write_enable) begin
            pc <= pc_next;
        end
    end

    assign pc_plus4 = pc + 32'd4;
    assign if_instr = instr_mem[pc[9:2]];
    
    // PC selection is handled after branch control in the EX stage.
    // (pc_next is updated below to include branch target logic)

    //========================================================================
    // IF/ID Pipeline Register with Flush and Write Enable
    //========================================================================
    logic [31:0] if_id_pc;
    logic [31:0] if_id_pc_plus4;
    logic [31:0] if_id_instr;
    logic        if_id_branch_flush_pending;

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            if_id_pc           <= 32'd0;
            if_id_pc_plus4     <= 32'd0;
            if_id_instr        <= 32'd0;
            if_id_branch_flush_pending <= 1'b0;
        end else if (flush_if_id) begin
            // Flush: insert NOP (zero all controls)
            if_id_pc           <= if_id_pc;  // Keep PC for reference
            if_id_pc_plus4     <= if_id_pc_plus4;
            if_id_instr        <= 32'd0;    // NOP encoding
            if_id_branch_flush_pending <= 1'b0;
        end else if (if_id_write_enable) begin
            // Normal update
            if_id_pc           <= pc;
            if_id_pc_plus4     <= pc_plus4;
            if_id_instr        <= if_instr;
            if_id_branch_flush_pending <= 1'b0;
        end
    end

    //========================================================================
    // ID Stage (Decode)
    //========================================================================
    logic [4:0]  id_rs1, id_rs2, id_rd;
    logic [31:0] id_imm;
    logic [3:0]  id_alu_op;
    logic        id_alu_src, id_mem_read, id_mem_write, id_mem_to_reg;
    logic        id_reg_write, id_branch, id_branch_type, id_jump;
    logic [31:0] id_rs1_data, id_rs2_data;

    rv32i_decoder id_decoder (
        .instr         (if_id_instr),
        .rs1           (id_rs1),
        .rs2           (id_rs2),
        .rd            (id_rd),
        .imm           (id_imm),
        .alu_op        (id_alu_op),
        .reg_write     (id_reg_write),
        .mem_read      (id_mem_read),
        .mem_write     (id_mem_write),
        .mem_to_reg    (id_mem_to_reg),
        .alu_src       (id_alu_src),
        .branch        (id_branch),
        .branch_type   (id_branch_type),
        .jump          (id_jump)
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
    // ID/EX Pipeline Register with Bubble Insertion and Flush
    //========================================================================
    logic [31:0] id_ex_pc;
    logic [31:0] id_ex_pc_plus4;
    logic [31:0] id_ex_instr;      // full instruction word — used by assertions
    logic [4:0]  id_ex_rs1, id_ex_rs2, id_ex_rd;
    logic [31:0] id_ex_rs1_data, id_ex_rs2_data, id_ex_imm;
    logic [3:0]  id_ex_alu_op;
    logic        id_ex_alu_src, id_ex_mem_read, id_ex_mem_write;
    logic        id_ex_mem_to_reg, id_ex_reg_write, id_ex_branch;
    logic        id_ex_branch_type, id_ex_jump;
    logic        insert_bubble;     // From hazard detection (stall)
    logic        flush_id_ex;       // From branch control

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            id_ex_pc           <= 32'd0;
            id_ex_pc_plus4     <= 32'd0;
            id_ex_instr        <= 32'd0;
            id_ex_rs1          <= 5'd0;
            id_ex_rs2          <= 5'd0;
            id_ex_rd           <= 5'd0;
            id_ex_rs1_data     <= 32'd0;
            id_ex_rs2_data     <= 32'd0;
            id_ex_imm          <= 32'd0;
            id_ex_alu_op       <= 4'd0;
            id_ex_alu_src      <= 1'b0;
            id_ex_mem_read     <= 1'b0;
            id_ex_mem_write    <= 1'b0;
            id_ex_mem_to_reg   <= 1'b0;
            id_ex_reg_write    <= 1'b0;
            id_ex_branch       <= 1'b0;
            id_ex_branch_type  <= 1'b0;
            id_ex_jump         <= 1'b0;
        end else if (flush_id_ex) begin
            // Flush from branch/jump: zero all controls (NOP bubble)
            id_ex_pc           <= 32'd0;
            id_ex_pc_plus4     <= 32'd0;
            id_ex_instr        <= 32'd0;
            id_ex_rs1          <= 5'd0;
            id_ex_rs2          <= 5'd0;
            id_ex_rd           <= 5'd0;
            id_ex_rs1_data     <= 32'd0;
            id_ex_rs2_data     <= 32'd0;
            id_ex_imm          <= 32'd0;
            id_ex_alu_op       <= 4'd0;
            id_ex_alu_src      <= 1'b0;
            id_ex_mem_read     <= 1'b0;
            id_ex_mem_write    <= 1'b0;
            id_ex_mem_to_reg   <= 1'b0;
            id_ex_reg_write    <= 1'b0;
            id_ex_branch       <= 1'b0;
            id_ex_branch_type  <= 1'b0;
            id_ex_jump         <= 1'b0;
        end else if (insert_bubble) begin
            // Stall bubble: zero controls (load-use hazard)
            id_ex_pc           <= 32'd0;
            id_ex_pc_plus4     <= 32'd0;
            id_ex_instr        <= 32'd0;
            id_ex_rs1          <= 5'd0;
            id_ex_rs2          <= 5'd0;
            id_ex_rd           <= 5'd0;
            id_ex_rs1_data     <= 32'd0;
            id_ex_rs2_data     <= 32'd0;
            id_ex_imm          <= 32'd0;
            id_ex_alu_op       <= 4'd0;
            id_ex_alu_src      <= 1'b0;
            id_ex_mem_read     <= 1'b0;
            id_ex_mem_write    <= 1'b0;
            id_ex_mem_to_reg   <= 1'b0;
            id_ex_reg_write    <= 1'b0;
            id_ex_branch       <= 1'b0;
            id_ex_branch_type  <= 1'b0;
            id_ex_jump         <= 1'b0;
        end else begin
            // Normal update
            id_ex_pc           <= if_id_pc;
            id_ex_pc_plus4     <= if_id_pc_plus4;
            id_ex_instr        <= if_id_instr;
            id_ex_rs1          <= id_rs1;
            id_ex_rs2          <= id_rs2;
            id_ex_rd           <= id_rd;
            id_ex_rs1_data     <= id_rs1_data;
            id_ex_rs2_data     <= id_rs2_data;
            id_ex_imm          <= id_imm;
            id_ex_alu_op       <= id_alu_op;
            id_ex_alu_src      <= id_alu_src;
            id_ex_mem_read     <= id_mem_read;
            id_ex_mem_write    <= id_mem_write;
            id_ex_mem_to_reg   <= id_mem_to_reg;
            id_ex_reg_write    <= id_reg_write;
            id_ex_branch       <= id_branch;
            id_ex_branch_type  <= id_branch_type;
            id_ex_jump         <= id_jump;
        end
    end

    //========================================================================
    // Hazard Detection Unit (for load-use stalls)
    //========================================================================
    logic hz_pc_write_en; // Raw stall-gated enable from hazard unit (no control-transfer override)
    rv32i_hazard_detection hz_detect (
        .id_rs1            (id_rs1),
        .id_rs2            (id_rs2),
        .ex_rd             (id_ex_rd),
        .ex_memread        (id_ex_mem_read),
        .stall             (),
        .pc_write_enable   (hz_pc_write_en),
        .if_id_write_enable(if_id_write_enable),
        .insert_bubble     (insert_bubble)
    );

    //========================================================================
    // EX Stage with Forwarding and Branch Execution
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
            2'b10: ex_alu_in1 = ex_mem_forward_data;  // pc_plus4 if JAL, alu_result otherwise
            default: ex_alu_in1 = id_ex_rs1_data;
        endcase
    end

    // ALU input B mux.
    // When alu_src=1 (SW, I-type), the ALU needs the immediate for address / operand
    // calculation. Forward_b must NOT override the immediate in that case, so the
    // ternary guard `id_ex_alu_src ? id_ex_imm : ...` is applied for non-zero forward
    // selections. This fully decouples the ALU address path from rs2 data forwarding.
    always_comb begin
        case (forward_b)
            2'b00: begin
                if (id_ex_alu_src)
                    ex_alu_in2 = id_ex_imm;
                else
                    ex_alu_in2 = id_ex_rs2_data;
            end
            2'b01: ex_alu_in2 = id_ex_alu_src ? id_ex_imm : wb_write_data;
            2'b10: ex_alu_in2 = id_ex_alu_src ? id_ex_imm : ex_mem_forward_data; // pc_plus4 if JAL
            default: ex_alu_in2 = id_ex_rs2_data;
        endcase
    end

    // Store data forwarding mux.
    // Mirrors forward_b priority but feeds ex_mem_rs2_data (the value written to
    // memory by SW), NOT the ALU. The ALU and store-data paths are now independent:
    //   ALU  path : ex_alu_in2 = base_addr + immediate  (always correct for SW)
    //   Store path: forwarded_rs2_data = most-recent rs2 value (possibly forwarded)
    always_comb begin
        case (forward_b)
            2'b10:   forwarded_rs2_data = ex_mem_forward_data; // MEM stage (pc_plus4 if JAL)
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
    // Branch Control Unit
    //========================================================================
    logic [31:0] pc_plus4_ex;
    
    assign pc_plus4_ex = id_ex_pc_plus4;  // PC+4 already calculated in previous stage

    rv32i_branch_control branch_ctrl (
        .branch         (id_ex_branch),
        .branch_type    (id_ex_branch_type),
        .zero           (ex_zero),
        .pc_ex          (id_ex_pc),
        .imm_ex         (id_ex_imm),
        .pc_normal      (pc_plus4_ex),
        .branch_taken   (branch_taken),
        .pc_next        (branch_pc_next),
        .flush_if_id    (br_flush_if_id),
        .flush_id_ex    (br_flush_id_ex)
    );
    
    // Jump logic in EX stage (combinational)
    assign jump_taken  = id_ex_jump ? 1'b1 : 1'b0;
    assign jump_target = id_ex_pc + id_ex_imm;
    
    // PC mux: jump > branch > normal
    assign pc_next = jump_taken ? jump_target : (branch_taken ? branch_pc_next : pc_plus4);
    
    // Combined flush signals (jump flushes override)
    assign flush_if_id = jump_taken ? 1'b1 : br_flush_if_id;
    assign flush_id_ex = jump_taken ? 1'b1 : br_flush_id_ex;

    // PC write enable:
    //   - Normally gated by hazard unit (~load_use_hazard)
    //   - branch_taken / jump_taken override the stall gate so the redirect target
    //     is never lost when both a load-use stall and a control transfer fire in
    //     the same cycle (the fetched instruction behind the branch is being
    //     flushed anyway, so freezing PC would discard the branch target silently)
    assign pc_write_enable = hz_pc_write_en | branch_taken | jump_taken;



    //========================================================================
    // EX/MEM Pipeline Register
    //========================================================================
    logic [31:0] ex_mem_pc_plus4;      // PC+4 buffer (for JAL link address)
    logic [31:0] ex_mem_alu_result;
    logic [31:0] ex_mem_rs2_data;
    logic [4:0]  ex_mem_rd;
    logic        ex_mem_mem_read, ex_mem_mem_write;
    logic        ex_mem_mem_to_reg, ex_mem_reg_write;
    logic        ex_mem_jump;

    // Forwarding data from MEM stage.
    // When a JAL is in MEM, ex_mem_alu_result = rs1_data + imm_j (wrong: ALU A input
    // is a register, not the PC). The correct link address is ex_mem_pc_plus4.
    // All three forwarding muxes use this wire; memory address/data paths keep
    // ex_mem_alu_result directly so they are not affected.
    wire [31:0] ex_mem_forward_data = ex_mem_jump ? ex_mem_pc_plus4 : ex_mem_alu_result;

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
            ex_mem_jump        <= 1'b0;
        end else begin
            ex_mem_pc_plus4    <= id_ex_pc_plus4;  // Propagate PC+4 buffer
            ex_mem_alu_result  <= ex_alu_result;
            ex_mem_rs2_data    <= forwarded_rs2_data;
            ex_mem_rd          <= id_ex_rd;
            ex_mem_mem_read    <= id_ex_mem_read;
            ex_mem_mem_write   <= id_ex_mem_write;
            ex_mem_mem_to_reg  <= id_ex_mem_to_reg;
            ex_mem_reg_write   <= id_ex_reg_write;
            ex_mem_jump        <= id_ex_jump;
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
    logic        mem_wb_jump;
    logic [31:0] mem_wb_pc_plus4;      // PC+4 propagated for JAL link address

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            mem_wb_alu_result  <= 32'd0;
            mem_wb_dmem_data   <= 32'd0;
            mem_wb_rd          <= 5'd0;
            mem_wb_mem_to_reg  <= 1'b0;
            mem_wb_reg_write   <= 1'b0;
            mem_wb_jump        <= 1'b0;
            mem_wb_pc_plus4    <= 32'd0;
        end else begin
            mem_wb_alu_result  <= ex_mem_alu_result;
            mem_wb_dmem_data   <= mem_dmem_rdata;
            mem_wb_rd          <= ex_mem_rd;
            mem_wb_mem_to_reg  <= ex_mem_mem_to_reg;
            mem_wb_reg_write   <= ex_mem_reg_write;
            mem_wb_jump        <= ex_mem_jump;
            mem_wb_pc_plus4    <= ex_mem_pc_plus4;  // Buffer PC+4 through MEM/WB
        end
    end

    //========================================================================
    // WB Stage
    //========================================================================
    logic [31:0] wb_write_data;

    // Priority mux: jump (PC+4) > load (dmem) > ALU result
    assign wb_write_data = mem_wb_jump ? mem_wb_pc_plus4 :
                           (mem_wb_mem_to_reg ? mem_wb_dmem_data : mem_wb_alu_result);

    //========================================================================
    // Outputs
    //========================================================================
    assign alu_result = ex_alu_result;
    assign zero = ex_zero;

endmodule
