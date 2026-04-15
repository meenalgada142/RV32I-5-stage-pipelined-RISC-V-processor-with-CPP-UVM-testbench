//============================================================================
// tb_forwarding_unit.sv
// Testbench for forwarding unit
//============================================================================

module tb_forwarding_unit;
    logic [4:0]  ex_rs1, ex_rs2;
    logic [4:0]  mem_rd;
    logic        mem_regwrite;
    logic [4:0]  wb_rd;
    logic        wb_regwrite;
    
    logic [1:0]  forward_a, forward_b;
    
    // DUT
    rv32i_forwarding_unit dut (
        .ex_rs1        (ex_rs1),
        .ex_rs2        (ex_rs2),
        .mem_rd        (mem_rd),
        .mem_regwrite  (mem_regwrite),
        .wb_rd         (wb_rd),
        .wb_regwrite   (wb_regwrite),
        .forward_a     (forward_a),
        .forward_b     (forward_b)
    );
    
    // Test cases
    initial begin
        $display("=== Forwarding Unit Testbench ===\n");
        
        // Test 1: No hazard (no forwarding needed)
        $display("Test 1: No hazard");
        ex_rs1 = 5'd1; ex_rs2 = 5'd2;
        mem_rd = 5'd3; mem_regwrite = 1'b1;
        wb_rd  = 5'd4; wb_regwrite = 1'b1;
        #10;
        assert(forward_a == 2'b00) else $error("forward_a should be 00");
        assert(forward_b == 2'b00) else $error("forward_b should be 00");
        $display("  ✓ forward_a=%b, forward_b=%b (both 00 - no hazard)\n", 
                 forward_a, forward_b);
        
        // Test 2: Forward from MEM (operand A)
        $display("Test 2: Forward from MEM stage");
        ex_rs1 = 5'd3; ex_rs2 = 5'd2;
        mem_rd = 5'd3; mem_regwrite = 1'b1;
        wb_rd  = 5'd4; wb_regwrite = 1'b1;
        #10;
        assert(forward_a == 2'b10) else $error("forward_a should be 10 (MEM)");
        assert(forward_b == 2'b00) else $error("forward_b should be 00");
        $display("  ✓ forward_a=%b (MEM), forward_b=%b (regfile)\n", 
                 forward_a, forward_b);
        
        // Test 3: Forward from MEM (operand B)
        $display("Test 3: Forward from MEM stage (other operand)");
        ex_rs1 = 5'd1; ex_rs2 = 5'd3;
        mem_rd = 5'd3; mem_regwrite = 1'b1;
        wb_rd  = 5'd4; wb_regwrite = 1'b1;
        #10;
        assert(forward_a == 2'b00) else $error("forward_a should be 00");
        assert(forward_b == 2'b10) else $error("forward_b should be 10 (MEM)");
        $display("  ✓ forward_a=%b (regfile), forward_b=%b (MEM)\n", 
                 forward_a, forward_b);
        
        // Test 4: Forward from WB (operand A)
        $display("Test 4: Forward from WB stage");
        ex_rs1 = 5'd4; ex_rs2 = 5'd2;
        mem_rd = 5'd3; mem_regwrite = 1'b1;
        wb_rd  = 5'd4; wb_regwrite = 1'b1;
        #10;
        assert(forward_a == 2'b01) else $error("forward_a should be 01 (WB)");
        assert(forward_b == 2'b00) else $error("forward_b should be 00");
        $display("  ✓ forward_a=%b (WB), forward_b=%b (regfile)\n", 
                 forward_a, forward_b);
        
        // Test 5: Forward from WB (operand B)
        $display("Test 5: Forward from WB stage (other operand)");
        ex_rs1 = 5'd1; ex_rs2 = 5'd4;
        mem_rd = 5'd3; mem_regwrite = 1'b1;
        wb_rd  = 5'd4; wb_regwrite = 1'b1;
        #10;
        assert(forward_a == 2'b00) else $error("forward_a should be 00");
        assert(forward_b == 2'b01) else $error("forward_b should be 01 (WB)");
        $display("  ✓ forward_a=%b (regfile), forward_b=%b (WB)\n", 
                 forward_a, forward_b);
        
        // Test 6: MEM priority over WB (same register)
        $display("Test 6: MEM priority over WB");
        ex_rs1 = 5'd3; ex_rs2 = 5'd3;
        mem_rd = 5'd3; mem_regwrite = 1'b1;
        wb_rd  = 5'd3; wb_regwrite = 1'b1;
        #10;
        assert(forward_a == 2'b10) else $error("forward_a should be 10 (MEM priority)");
        assert(forward_b == 2'b10) else $error("forward_b should be 10 (MEM priority)");
        $display("  ✓ Both forward from MEM (priority over WB)\n");
        
        // Test 7: Disable MEM write, use WB
        $display("Test 7: MEM write disabled, WB available");
        ex_rs1 = 5'd3; ex_rs2 = 5'd2;
        mem_rd = 5'd3; mem_regwrite = 1'b0;  // ← regwrite disabled
        wb_rd  = 5'd3; wb_regwrite = 1'b1;
        #10;
        assert(forward_a == 2'b01) else $error("forward_a should be 01 (WB)");
        assert(forward_b == 2'b00) else $error("forward_b should be 00");
        $display("  ✓ forward_a=%b (WB), forward_b=%b (regfile)\n", 
                 forward_a, forward_b);
        
        // Test 8: Never forward x0
        $display("Test 8: Never forward x0 (hardwired zero)");
        ex_rs1 = 5'd0; ex_rs2 = 5'd0;
        mem_rd = 5'd0; mem_regwrite = 1'b1;
        wb_rd  = 5'd0; wb_regwrite = 1'b1;
        #10;
        assert(forward_a == 2'b00) else $error("forward_a should be 00 (never forward x0)");
        assert(forward_b == 2'b00) else $error("forward_b should be 00 (never forward x0)");
        $display("  ✓ No forwarding for x0 even with regwrite=1\n");
        
        // Test 9: ALU-ALU dependency (ADD → SUB scenario)
        $display("Test 9: ALU-ALU dependency (ADD → SUB)");
        // ADD x3, x1, x2 in MEM
        // SUB x4, x3, x5 in EX
        ex_rs1 = 5'd3; ex_rs2 = 5'd5;  // SUB needs x3
        mem_rd = 5'd3; mem_regwrite = 1'b1;  // ADD writes x3
        wb_rd  = 5'd6; wb_regwrite = 1'b1;   // Previous instr writes x6
        #10;
        assert(forward_a == 2'b10) else $error("forward_a should be 10 (MEM)");
        assert(forward_b == 2'b00) else $error("forward_b should be 00 (x5 in regfile)");
        $display("  ✓ Hazard detected: forward x3 from MEM stage\n");
        
        // Test 10: Complex scenario (multiple sources)
        $display("Test 10: Complex scenario (different results for A and B)");
        // Instruction needs both x3 and x4
        // x3 available from MEM
        // x4 available from WB
        ex_rs1 = 5'd3; ex_rs2 = 5'd4;
        mem_rd = 5'd3; mem_regwrite = 1'b1;
        wb_rd  = 5'd4; wb_regwrite = 1'b1;
        #10;
        assert(forward_a == 2'b10) else $error("forward_a should be 10 (x3 from MEM)");
        assert(forward_b == 2'b01) else $error("forward_b should be 01 (x4 from WB)");
        $display("  ✓ forward_a=%b (MEM), forward_b=%b (WB)\n", 
                 forward_a, forward_b);
        
        // Test 11: Both disabled (should never happen in real design)
        $display("Test 11: Both writes disabled");
        ex_rs1 = 5'd1; ex_rs2 = 5'd2;
        mem_rd = 5'd1; mem_regwrite = 1'b0;
        wb_rd  = 5'd2; wb_regwrite = 1'b0;
        #10;
        assert(forward_a == 2'b00) else $error("forward_a should be 00");
        assert(forward_b == 2'b00) else $error("forward_b should be 00");
        $display("  ✓ No forwarding when both writes disabled\n");
        
        // Test 12: WB writes x0 (should be ignored)
        $display("Test 12: WB tries to write x0 (ignored)");
        ex_rs1 = 5'd0; ex_rs2 = 5'd0;
        mem_rd = 5'd3; mem_regwrite = 1'b0;
        wb_rd  = 5'd0; wb_regwrite = 1'b1;  // Tries to write x0
        #10;
        assert(forward_a == 2'b00) else $error("forward_a should be 00");
        assert(forward_b == 2'b00) else $error("forward_b should be 00");
        $display("  ✓ No forwarding from x0 write\n");
        
        $display("=== All tests passed! ===\n");
        $finish;
    end

endmodule
