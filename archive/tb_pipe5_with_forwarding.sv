//============================================================================
// tb_pipe5_with_forwarding.sv
// Testbench for 5-stage pipeline WITH forwarding - hazard verification
//============================================================================

module tb_pipe5_with_forwarding;
    logic clk;
    logic rst;
    logic [31:0] alu_result;
    logic zero;

    rv32i_pipe5_with_forwarding dut (
        .clk(clk),
        .rst(rst),
        .alu_result(alu_result),
        .zero(zero)
    );

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    // Program loading
    task load_program_hazard_test();
        $display("\n=== Loading Program: ALU-ALU Hazard Test ===\n");
        
        // Simple ALU chain exploiting forwarding
        dut.instr_mem[0] = 32'b000000000101_00000_000_00001_0010011;  // ADDI x1, x0, 5
        dut.instr_mem[1] = 32'b000000000011_00000_000_00010_0010011;  // ADDI x2, x0, 3
        dut.instr_mem[2] = 32'b0000000_00010_00001_000_00011_0110011; // ADD x3, x1, x2  (no hazard)
        dut.instr_mem[3] = 32'b0000000_00011_00011_000_00100_0110011; // ADD x4, x3, x3  (uses x3 from MEM)
        dut.instr_mem[4] = 32'b0000000_00100_00100_000_00101_0110011; // ADD x5, x4, x4  (uses x4 from MEM or WB)
        dut.instr_mem[5] = 32'b0000000_00010_00001_111_00110_0110011; // AND x6, x1, x2  (no hazard)
        dut.instr_mem[6] = 32'b0000000_00010_00001_110_00111_0110011; // OR  x7, x1, x2  (no hazard)
        dut.instr_mem[7] = 32'b0000000_00010_00001_100_01000_0110011; // XOR x8, x1, x2  (no hazard)
        
        $display("Program loaded:");
        $display("  [0] ADDI x1, x0, 5");
        $display("  [1] ADDI x2, x0, 3");
        $display("  [2] ADD x3, x1, x2   (no hazard: x1, x2 in regfile)");
        $display("  [3] ADD x4, x3, x3   ← HAZARD: x3 from instr[2] (resolves in cycle 6)");
        $display("  [4] ADD x5, x4, x4   ← HAZARD: x4 from instr[3]");
        $display("  [5] AND x6, x1, x2   (no hazard: x1, x2 in regfile)");
        $display("  [6] OR  x7, x1, x2   (no hazard: x1, x2 in regfile)");
        $display("  [7] XOR x8, x1, x2   (no hazard: x1, x2 in regfile)");
        
        $display("\nExpected Results with Forwarding:");
        $display("  x1 = 5");
        $display("  x2 = 3");
        $display("  x3 = 8   (5 + 3)");
        $display("  x4 = 16  (8 + 8) ← Uses forwarded x3 from MEM");
        $display("  x5 = 32  (16 + 16) ← Uses forwarded x4 from MEM");
        $display("  x6 = 1   (5 & 3)");
        $display("  x7 = 7   (5 | 3)");
        $display("  x8 = 6   (5 ^ 3)\n");
    endtask

    task load_program_simple();
        $display("\n=== Loading Program: Simple Test (Independent Instructions) ===\n");
        
        dut.instr_mem[0] = 32'b000000000101_00000_000_00001_0010011;  // ADDI x1, x0, 5
        dut.instr_mem[1] = 32'b000000000011_00000_000_00010_0010011;  // ADDI x2, x0, 3
        dut.instr_mem[2] = 32'b000000000001_00000_000_00011_0010011;  // ADDI x3, x0, 1
        dut.instr_mem[3] = 32'b000000000010_00000_000_00100_0010011;  // ADDI x4, x0, 2
        dut.instr_mem[4] = 32'b0000000_00010_00001_000_00101_0110011; // ADD x5, x1, x2
        dut.instr_mem[5] = 32'b0000000_00100_00011_000_00110_0110011; // ADD x6, x3, x4
        dut.instr_mem[6] = 32'b0000000_00010_00001_111_00111_0110011; // AND x7, x1, x2
        dut.instr_mem[7] = 32'b0000000_00010_00001_110_01000_0110011; // OR  x8, x1, x2
        
        $display("Expected Results:");
        $display("  x1 = 5, x2 = 3, x3 = 1, x4 = 2");
        $display("  x5 = 8 (5+3), x6 = 3 (1+2)");
        $display("  x7 = 1 (5&3), x8 = 7 (5|3)\n");
    endtask

    // Test sequence
    initial begin
        rst = 1;
        #10 rst = 0;
        
        // Run simple test first
        load_program_simple();
        repeat(15) @(posedge clk);
        
        $display("\n=== Switching to Hazard Test ===");
        
        // Reload for hazard test
        repeat(5) @(posedge clk);
        rst = 1;
        #10 rst = 0;
        
        load_program_hazard_test();
        repeat(25) @(posedge clk);
        
        // Print final results
        $display("\n=== Final Register State ===");
        $display("x0 = %h (always 0)", dut.id_regfile.regs[0]);
        for (int i=1; i<=8; i++) begin
            $display("x%d = %h", i, dut.id_regfile.regs[i]);
        end
        $display("");
        
        // Verification
        $display("=== Verification ===");
        if (dut.id_regfile.regs[1] == 32'd5) $display("✓ x1 = 5 (ADDI)");
        else $display("✗ x1 != 5, got %d", dut.id_regfile.regs[1]);
        
        if (dut.id_regfile.regs[2] == 32'd3) $display("✓ x2 = 3 (ADDI)");
        else $display("✗ x2 != 3, got %d", dut.id_regfile.regs[2]);
        
        if (dut.id_regfile.regs[3] == 32'd8) $display("✓ x3 = 8 (5+3, ADD)");
        else $display("✗ x3 != 8, got %d", dut.id_regfile.regs[3]);
        
        if (dut.id_regfile.regs[4] == 32'd16) $display("✓ x4 = 16 (8+8, forwarded)");
        else $display("✗ x4 != 16, got %d (forwarding may not be working)", dut.id_regfile.regs[4]);
        
        if (dut.id_regfile.regs[5] == 32'd32) $display("✓ x5 = 32 (16+16, forwarded)");
        else $display("✗ x5 != 32, got %d (forwarding may not be working)", dut.id_regfile.regs[5]);
        
        if (dut.id_regfile.regs[6] == 32'd1) $display("✓ x6 = 1 (5&3, AND)");
        else $display("✗ x6 != 1, got %d", dut.id_regfile.regs[6]);
        
        if (dut.id_regfile.regs[7] == 32'd7) $display("✓ x7 = 7 (5|3, OR)");
        else $display("✗ x7 != 7, got %d", dut.id_regfile.regs[7]);
        
        if (dut.id_regfile.regs[8] == 32'd6) $display("✓ x8 = 6 (5^3, XOR)");
        else $display("✗ x8 != 6, got %d", dut.id_regfile.regs[8]);
        
        $display("\n=== Test Complete ===\n");
        $finish;
    end

endmodule
