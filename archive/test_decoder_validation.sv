module test_decoder_comprehensive;
    logic [31:0] instr;
    logic [3:0]  alu_op;
    logic        reg_write, alu_src;
    logic [31:0] imm;

    rv32i_decoder dec (
        .instr(instr),
        .alu_op(alu_op),
        .reg_write(reg_write),
        .alu_src(alu_src),
        .imm(imm),
        .rs1(), .rs2(), .rd(), 
        .mem_read(), .mem_write(), .mem_to_reg(), 
        .branch(), .branch_type(), .jump()
    );

    initial begin
        $display("\n╔════════════════════════════════════════════════════════════════╗");
        $display("║           RV32I DECODER VALIDATION TEST                         ║");
        $display("╚════════════════════════════════════════════════════════════════╝\n");

        // =====================================================================
        // R-TYPE INSTRUCTIONS
        // =====================================================================
        $display("R-TYPE INSTRUCTIONS (opcode=0110011, alu_src=0):");
        $display("─────────────────────────────────────────────────────────────────\n");

        // ADD x1, x2, x3: funct7=0, funct3=000
        instr = 32'b0000000_00011_00010_000_00001_0110011;
        #1;
        $display("ADD x1, x2, x3:");
        $display("  alu_op=%b (expect 0000) | alu_src=%b (expect 0) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0000 && alu_src==1'b0) ? "✓" : "✗");

        // SUB x1, x2, x3: funct7=1, funct3=000
        instr = 32'b0100000_00011_00010_000_00001_0110011;
        #1;
        $display("SUB x1, x2, x3:");
        $display("  alu_op=%b (expect 0001) | alu_src=%b (expect 0) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0001 && alu_src==1'b0) ? "✓" : "✗");

        // SLT x1, x2, x3: funct7=0, funct3=010
        instr = 32'b0000000_00011_00010_010_00001_0110011;
        #1;
        $display("SLT x1, x2, x3 (signed <):");
        $display("  alu_op=%b (expect 0101) | alu_src=%b (expect 0) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0101 && alu_src==1'b0) ? "✓" : "✗");

        // SLTU x1, x2, x3: funct7=0, funct3=011
        instr = 32'b0000000_00011_00010_011_00001_0110011;
        #1;
        $display("SLTU x1, x2, x3 (unsigned <):");
        $display("  alu_op=%b (expect 1000) | alu_src=%b (expect 0) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b1000 && alu_src==1'b0) ? "✓" : "✗");

        // SLL x1, x2, x3: funct7=0, funct3=001
        instr = 32'b0000000_00011_00010_001_00001_0110011;
        #1;
        $display("SLL x1, x2, x3 (shift left):");
        $display("  alu_op=%b (expect 0110) | alu_src=%b (expect 0) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0110 && alu_src==1'b0) ? "✓" : "✗");

        // SRL x1, x2, x3: funct7=0, funct3=101
        instr = 32'b0000000_00011_00010_101_00001_0110011;
        #1;
        $display("SRL x1, x2, x3 (logical right):");
        $display("  alu_op=%b (expect 0111) | alu_src=%b (expect 0) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0111 && alu_src==1'b0) ? "✓" : "✗");

        // SRA x1, x2, x3: funct7=1, funct3=101
        instr = 32'b0100000_00011_00010_101_00001_0110011;
        #1;
        $display("SRA x1, x2, x3 (arithmetic right):");
        $display("  alu_op=%b (expect 1001) | alu_src=%b (expect 0) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b1001 && alu_src==1'b0) ? "✓" : "✗");

        // =====================================================================
        // I-TYPE INSTRUCTIONS
        // =====================================================================
        $display("I-TYPE INSTRUCTIONS (opcode=0010011, alu_src=1):");
        $display("─────────────────────────────────────────────────────────────────\n");

        // ADDI x1, x2, 10: funct3=000
        instr = 32'b000000001010_00010_000_00001_0010011;
        #1;
        $display("ADDI x1, x2, 10:");
        $display("  alu_op=%b (expect 0000) | alu_src=%b (expect 1) | imm=%d | PASS=%s\n",
                 alu_op, alu_src, $signed(imm), (alu_op==4'b0000 && alu_src==1'b1 && imm==32'd10) ? "✓" : "✗");

        // SLTI x1, x2, 5: funct3=010 (signed <)
        instr = 32'b000000000101_00010_010_00001_0010011;
        #1;
        $display("SLTI x1, x2, 5 (signed <):");
        $display("  alu_op=%b (expect 0101) | alu_src=%b (expect 1) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0101 && alu_src==1'b1) ? "✓" : "✗");

        // SLTIU x1, x2, 5: funct3=011 (unsigned <)
        instr = 32'b000000000101_00010_011_00001_0010011;
        #1;
        $display("SLTIU x1, x2, 5 (unsigned <):");
        $display("  alu_op=%b (expect 1000) | alu_src=%b (expect 1) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b1000 && alu_src==1'b1) ? "✓" : "✗");

        // XORI x1, x2, 7: funct3=100
        instr = 32'b000000000111_00010_100_00001_0010011;
        #1;
        $display("XORI x1, x2, 7:");
        $display("  alu_op=%b (expect 0100) | alu_src=%b (expect 1) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0100 && alu_src==1'b1) ? "✓" : "✗");

        // ORI x1, x2, 3: funct3=110
        instr = 32'b000000000011_00010_110_00001_0010011;
        #1;
        $display("ORI x1, x2, 3:");
        $display("  alu_op=%b (expect 0011) | alu_src=%b (expect 1) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0011 && alu_src==1'b1) ? "✓" : "✗");

        // ANDI x1, x2, 0xF: funct3=111
        instr = 32'b000000001111_00010_111_00001_0010011;
        #1;
        $display("ANDI x1, x2, 0xF:");
        $display("  alu_op=%b (expect 0010) | alu_src=%b (expect 1) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0010 && alu_src==1'b1) ? "✓" : "✗");

        // SLLI x1, x2, 2: funct3=001
        instr = 32'b000000000010_00010_001_00001_0010011;
        #1;
        $display("SLLI x1, x2, 2 (shift left imm):");
        $display("  alu_op=%b (expect 0110) | alu_src=%b (expect 1) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0110 && alu_src==1'b1) ? "✓" : "✗");

        // SRLI x1, x2, 3: funct3=101, funct7[5]=0
        instr = 32'b000000000011_00010_101_00001_0010011;
        #1;
        $display("SRLI x1, x2, 3 (logical right imm):");
        $display("  alu_op=%b (expect 0111) | alu_src=%b (expect 1) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b0111 && alu_src==1'b1) ? "✓" : "✗");

        // SRAI x1, x2, 3: funct3=101, funct7[5]=1
        instr = 32'b010000000011_00010_101_00001_0010011;
        #1;
        $display("SRAI x1, x2, 3 (arithmetic right imm):");
        $display("  alu_op=%b (expect 1001) | alu_src=%b (expect 1) | PASS=%s\n",
                 alu_op, alu_src, (alu_op==4'b1001 && alu_src==1'b1) ? "✓" : "✗");

        $display("\n╔════════════════════════════════════════════════════════════════╗");
        $display("║           VALIDATION COMPLETE                                  ║");
        $display("╚════════════════════════════════════════════════════════════════╝\n");
        
        $finish;
    end
endmodule
