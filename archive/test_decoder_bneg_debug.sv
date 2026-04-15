module test_decoder_bneg_debug;
    logic [31:0] instr;
    logic [31:0] imm, imm_b_test;
    logic [3:0] instr_11_8;
    logic instr_7;

    rv32i_decoder dec (.instr(instr), .imm(imm), .rs1(), .rs2(), .rd(), .alu_op(), .reg_write(), .mem_read(), .mem_write(), .mem_to_reg(), .alu_src(), .branch(), .branch_type(), .jump());

    initial begin
        instr = 32'b1111111_00000_00001_001_11000_1100011;
        
        instr_11_8 = instr[11:8];
        instr_7 = instr[7];
        
        #1;
        $display("Instruction = %h", instr);
        $display("instr[31] = %b", instr[31]);
        $display("instr[30:25] = %b", instr[30:25]);
        $display("instr[11:8] = %b", instr_11_8);
        $display("instr[7] = %b", instr_7);
        
        $display("\nDecoder output:");
        $display("imm = %h = %d", imm, $signed(imm));
        $display("Expected: FFFFFFF8 (-8)");
        $display("Actual vs Expected: %s", (imm == 32'hFFFFFFF8) ? "PASS" : "FAIL");
        
        // Try computing manually
        imm_b_test = {{19{instr[31]}}, instr[31], instr[30:25], instr[11:8], instr[7], 1'b0};
        $display("\nManual imm_b computation = %h = %d", imm_b_test, $signed(imm_b_test));

        $finish;
    end
endmodule
