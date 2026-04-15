module test_decoder_bneg;
    logic [31:0] instr;
    logic [31:0] imm;

    rv32i_decoder dec (.instr(instr), .imm(imm), .rs1(), .rs2(), .rd(), .alu_op(), .reg_write(), .mem_read(), .mem_write(), .mem_to_reg(), .alu_src(), .branch(), .branch_type(), .jump());

    initial begin
        // BNE x1, x0, -8 (offset -8, encoded as bits 1111111_00000_00001_001_11000_1100011)
        instr = 32'b1111111_00000_00001_001_11000_1100011;
        #1;
        $display("BNE x1, x0, -8:");
        $display("  instr = %h", instr);
        $display("  imm = %h (binary: %b)", imm, imm);
        $display("  expected: ffffffff8 (binary: ...11111000)");
        
        if (imm == 32'hFFFFFFF8)
            $display("  ✓ CORRECT!");
        else  
            $display("  ✗ WRONG! (off by %d)", imm - 32'hFFFFFFF8);

        $finish;
    end
endmodule
