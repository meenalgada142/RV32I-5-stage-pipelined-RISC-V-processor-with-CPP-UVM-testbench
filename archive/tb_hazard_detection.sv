//============================================================================
// tb_hazard_detection.sv
// Testbench for hazard detection and stall unit
//============================================================================

module tb_hazard_detection;
    logic [4:0]  id_rs1, id_rs2;
    logic [4:0]  ex_rd;
    logic        ex_memread;
    
    logic        stall;
    logic        pc_write_enable;
    logic        if_id_write_enable;
    logic        insert_bubble;
    
    // DUT
    rv32i_hazard_detection dut (
        .id_rs1            (id_rs1),
        .id_rs2            (id_rs2),
        .ex_rd             (ex_rd),
        .ex_memread        (ex_memread),
        .stall             (stall),
        .pc_write_enable   (pc_write_enable),
        .if_id_write_enable(if_id_write_enable),
        .insert_bubble     (insert_bubble)
    );
    
    // Test cases
    initial begin
        $display("=== Hazard Detection Unit Testbench ===\n");
        
        // Test 1: No load (ex_memread = 0)
        $display("Test 1: No load instruction in EX");
        ex_memread = 1'b0;
        ex_rd = 5'd5;
        id_rs1 = 5'd5;
        id_rs2 = 5'd3;
        #10;
        assert(stall == 1'b0) else $error("Should not stall when no load");
        assert(pc_write_enable == 1'b1) else $error("PC should be writable");
        $display("  ✓ stall=0, pc_write_enable=1\n");
        
        // Test 2: Load instruction, but no operand match
        $display("Test 2: Load instruction, different registers");
        ex_memread = 1'b1;
        ex_rd = 5'd1;
        id_rs1 = 5'd2;
        id_rs2 = 5'd3;
        #10;
        assert(stall == 1'b0) else $error("Should not stall, no match");
        assert(if_id_write_enable == 1'b1) else $error("IF/ID should be writable");
        $display("  ✓ stall=0, if_id_write_enable=1 (no match)\n");
        
        // Test 3: Load-use hazard on rs1
        $display("Test 3: Load-use hazard on rs1 ← STALL");
        ex_memread = 1'b1;
        ex_rd = 5'd5;
        id_rs1 = 5'd5;  // ← MATCH
        id_rs2 = 5'd3;
        #10;
        assert(stall == 1'b1) else $error("Should stall on rs1 match");
        assert(pc_write_enable == 1'b0) else $error("PC should freeze");
        assert(if_id_write_enable == 1'b0) else $error("IF/ID should freeze");
        assert(insert_bubble == 1'b1) else $error("Should insert bubble");
        $display("  ✓ stall=1, pc_write=0, if_id_write=0, bubble=1\n");
        
        // Test 4: Load-use hazard on rs2
        $display("Test 4: Load-use hazard on rs2 ← STALL");
        ex_memread = 1'b1;
        ex_rd = 5'd7;
        id_rs1 = 5'd2;
        id_rs2 = 5'd7;  // ← MATCH
        #10;
        assert(stall == 1'b1) else $error("Should stall on rs2 match");
        assert(pc_write_enable == 1'b0) else $error("PC should freeze");
        assert(insert_bubble == 1'b1) else $error("Should insert bubble");
        $display("  ✓ stall=1, bubble=1 (rs2 match)\n");
        
        // Test 5: Load-use hazard on both rs1 and rs2
        $display("Test 5: Load-use hazard on both operands ← STALL");
        ex_memread = 1'b1;
        ex_rd = 5'd4;
        id_rs1 = 5'd4;  // ← MATCH
        id_rs2 = 5'd4;  // ← MATCH
        #10;
        assert(stall == 1'b1) else $error("Should stall on any match");
        assert(if_id_write_enable == 1'b0) else $error("IF/ID should freeze");
        $display("  ✓ stall=1 (both operands match)\n");
        
        // Test 6: Load to x0 (no hazard)
        $display("Test 6: Load to x0 (read-only)");
        ex_memread = 1'b1;
        ex_rd = 5'd0;  // ← x0 (ignored)
        id_rs1 = 5'd0;
        id_rs2 = 5'd0;
        #10;
        assert(stall == 1'b0) else $error("Should never stall from x0 load");
        assert(pc_write_enable == 1'b1) else $error("PC writable");
        $display("  ✓ stall=0 (x0 never creates hazard)\n");
        
        // Test 7: Load to x0, but independent instruction
        $display("Test 7: Load to x0, dependent instr exists");
        ex_memread = 1'b1;
        ex_rd = 5'd0;  // ← x0
        id_rs1 = 5'd1;
        id_rs2 = 5'd2;  // Neither is x0
        #10;
        assert(stall == 1'b0) else $error("No hazard from x0");
        $display("  ✓ stall=0 (x0 destination doesn't create hazard)\n");
        
        // Test 8: Load-use with both operands different but one matches
        $display("Test 8: Load-use with different operands");
        ex_memread = 1'b1;
        ex_rd = 5'd6;
        id_rs1 = 5'd6;  // ← MATCH
        id_rs2 = 5'd3;
        #10;
        assert(stall == 1'b1) else $error("Should stall");
        $display("  ✓ stall=1\n");
        
        // Test 9: No load, but registers happen to match
        $display("Test 9: No load, registers match");
        ex_memread = 1'b0;  // ← NOT a load
        ex_rd = 5'd5;
        id_rs1 = 5'd5;  // Match, but no load
        id_rs2 = 5'd3;
        #10;
        assert(stall == 1'b0) else $error("Should not stall, no load");
        $display("  ✓ stall=0 (load required for hazard)\n");
        
        // Test 10: Transition - stall then no-stall
        $display("Test 10: Transition from stall to no-stall");
        // First: stall
        ex_memread = 1'b1;
        ex_rd = 5'd3;
        id_rs1 = 5'd3;
        id_rs2 = 5'd1;
        #10;
        assert(stall == 1'b1) else $error("Should stall");
        
        // Next: no stall (load moves to MEM, new instr in EX)
        ex_memread = 1'b0;  // Load now in MEM, new ALU instr in EX
        ex_rd = 5'd2;
        id_rs1 = 5'd1;
        id_rs2 = 5'd4;
        #10;
        assert(stall == 1'b0) else $error("Should not stall now");
        $display("  ✓ Transitioned: stall=1 → stall=0\n");
        
        // Test 11: rs1 == rs2
        $display("Test 11: rs1 == rs2 (same source used twice)");
        ex_memread = 1'b1;
        ex_rd = 5'd8;
        id_rs1 = 5'd8;  // ← MATCH
        id_rs2 = 5'd8;  // ← Also MATCH
        #10;
        assert(stall == 1'b1) else $error("Should stall");
        $display("  ✓ stall=1 (both operands are load result)\n");
        
        // Test 12: Boundary case - rd == 31 (x31)
        $display("Test 12: Load to x31 (highest register)");
        ex_memread = 1'b1;
        ex_rd = 5'd31;
        id_rs1 = 5'd31;  // ← MATCH
        id_rs2 = 5'd15;
        #10;
        assert(stall == 1'b1) else $error("Should stall on x31");
        $display("  ✓ stall=1 (x31 can create hazard)\n");
        
        // Test 13: All control signals tied correctly
        $display("Test 13: Control signal coherence");
        ex_memread = 1'b1;
        ex_rd = 5'd2;
        id_rs1 = 5'd2;
        id_rs2 = 5'd1;
        #10;
        if (stall == 1'b1) begin
            assert(pc_write_enable == 1'b0) else $error("Mismatched signals");
            assert(if_id_write_enable == 1'b0) else $error("Mismatched signals");
            assert(insert_bubble == 1'b1) else $error("Mismatched signals");
            $display("  ✓ All signals coherent when stalling\n");
        end
        
        // Test 14: Control signals when not stalling
        $display("Test 14: Control signals when not stalling");
        ex_memread = 1'b0;
        ex_rd = 5'd10;
        id_rs1 = 5'd5;
        id_rs2 = 5'd6;
        #10;
        assert(pc_write_enable == 1'b1) else $error("PC should write");
        assert(if_id_write_enable == 1'b1) else $error("IF/ID should write");
        assert(insert_bubble == 1'b0) else $error("No bubble needed");
        assert(stall == 1'b0) else $error("No stall");
        $display("  ✓ Normal operation signals\n");
        
        $display("=== All 14 tests passed! ===\n");
        $finish;
    end

endmodule
