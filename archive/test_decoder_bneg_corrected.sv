module test_decoder_bneg_corrected;
    logic [31:0] instr;
    logic [31:0] imm, imm_b_test;
    logic [3:0] instr_11_8;
    logic instr_7;

    rv32i_decoder dec (.instr(instr), .imm(imm), .rs1(), .rs2(), .rd(), .alu_op(), .reg_write(), .mem_read(), .mem_write(), .mem_to_reg(), .alu_src(), .branch(), .branch_type(), .jump());

    initial begin
        $display("\n=== B-FORMAT IMMEDIATE DECODING TEST (CORRECTED) ===\n");
        
        // BNE x1, x0, -8
        // For offset -8 in 13-bit signed: -8 = 2^13 - 8 = 8184 = 0x1FF8 = 1_1111_1111_1000
        // Breaking into B-format fields:
        //   offset[12] = 1      → instr[31]
        //   offset[10:5] = 111111 → instr[30:25]
        //   offset[4:1] = 1110  → instr[11:8]  (NOT 1100!)
        //   offset[11] = 1      → instr[7]     (NOT 0!)
        //   offset[0] = 0       (implicit)
        //
        // Correct instruction: 0xFE409EC3 (NOT 0xfe009c63)
        // Binary: 1111111_0_00000_00001_001_1110_1_1100011
        
        instr = 32'hFE409EC3;  // CORRECT ENCODING FOR -8
        
        instr_11_8 = instr[11:8];
        instr_7 = instr[7];
        
        #1;
        
        $display("========== CORRECTED INSTRUCTION ==========");
        $display("Hex Instruction = 0x%h (should be 0xFE409EC3)", instr);
        $display("Binary: %b", instr);
        $display("");
        $display("Field Extraction:");
        $display("  instr[31]    = %b (should be 1 for offset[12])", instr[31]);
        $display("  instr[30:25] = %b (should be 111111 for offset[10:5])", instr[30:25]);
        $display("  instr[11:8]  = %b (should be 1110 for offset[4:1])", instr_11_8);
        $display("  instr[7]     = %b (should be 1 for offset[11])", instr_7);
        $display("");
        
        $display("Decoder Output:");
        $display("  imm = 0x%h = %d (signed)", imm, $signed(imm));
        $display("  Expected: 0xFFFFFFF8 (decimal -8)");
        
        if (imm == 32'hFFFFFFF8)
            $display("  ✓ PASS - Decoder correctly outputs -8\n");
        else
            $display("  ✗ FAIL - Decoder output: 0x%h\n", imm);
        
        // Verify manual calculation
        imm_b_test = {{19{instr[31]}}, instr[31], instr[30:25], instr[11:8], instr[7], 1'b0};
        $display("Manual Verification:");
        $display("  {{19{instr[31]}}, instr[31], instr[30:25], instr[11:8], instr[7], 1'b0}");
        $display("  = %h = %d", imm_b_test, $signed(imm_b_test));
        
        if (imm_b_test == 32'hFFFFFFF8)
            $display("  ✓ Manual calc also correct\n");
        else
            $display("  ✗ Manual calc mismatch\n");

        $finish;
    end
endmodule
