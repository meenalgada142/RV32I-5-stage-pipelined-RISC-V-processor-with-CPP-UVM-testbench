module test_decoder_simple;
    logic [31:0] instr;
    logic [4:0]  rs1, rs2, rd;
    logic [31:0] imm;
    logic [3:0]  alu_op;
    logic        reg_write, branch, jump;
    logic        branch_type;

    rv32i_decoder dec (.instr(instr), .rs1(rs1), .rs2(rs2), .rd(rd), .imm(imm), 
                       .alu_op(alu_op), .reg_write(reg_write), .mem_read(), .mem_write(), 
                       .mem_to_reg(), .alu_src(), .branch(branch), .branch_type(branch_type), .jump(jump));

    initial begin
        // Test ADDI x1, x0, 5
        instr = 32'b000000000101_00000_000_00001_0010011;  // ADDI
        #1;
        $display("ADDI:  reg_write=%b (expect 1), rd=%d (expect 1)", reg_write, rd);

        // Test BEQ x1, x2, +0
        instr = 32'b0000000_00010_00001_000_00000_1100011;  // BEQ
        #1;
        $display("BEQ:   branch=%b (expect 1), rd=%d (expect 0)", branch, rd);

        $finish;
    end
endmodule
