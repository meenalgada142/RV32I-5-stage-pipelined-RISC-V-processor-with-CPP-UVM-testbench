////////////////////////////////////////////////////////////////////////////////
//  tb_pipe5_with_branches.sv
//
//  Comprehensive Testbench for Branch Support in 5-Stage Pipeline
//
//  Test Scenarios:
//  1. Forward branch taken
//  2. Backward branch (loop with conditional)
//  3. Branch not taken
//  4. Nested branches
//  5. Branch with forwarding interaction
//
//============================================================================

module tb_pipe5_with_branches;
    logic clk;
    logic rst;
    logic [31:0] alu_result;
    logic zero;

    rv32i_pipe5_with_branches dut (
        .clk(clk),
        .rst(rst),
        .alu_result(alu_result),
        .zero(zero)
    );

    integer cycle = 0;

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    task load_program_forward_branch_taken();
        $display("\n=== Program: Forward Branch Taken ===\n");
        
        // Program: SET x1=5, x2=5, BEQ (taken), skip wrong path, execute target
        dut.instr_mem[0]  = 32'b000000000101_00000_000_00001_0010011; // ADDI x1, x0, 5
        dut.instr_mem[1]  = 32'b000000000101_00000_000_00010_0010011; // ADDI x2, x0, 5
        dut.instr_mem[2]  = 32'b0000000_00010_00001_000_01100_1100011; // BEQ x1, x2, +12 (to PC=20=instr[5])
        dut.instr_mem[3]  = 32'b000000001100_00000_000_00011_0010011; // ADDI x3, x0, 12 (wrong path!)
        dut.instr_mem[4]  = 32'b000000001100_00000_000_00011_0010011; // ADDI x3, x0, 12 (wrong path!)
        dut.instr_mem[5]  = 32'b0000000_00010_00001_000_00100_0110011; // ADD x4, x1, x2 (target)
        dut.instr_mem[6]  = 32'b0000000_00100_00100_000_00101_0110011; // ADD x5, x4, x4
        
        $display("Program:");
        $display("  [0] ADDI x1, x0, 5           → x1 = 5");
        $display("  [1] ADDI x2, x0, 5           → x2 = 5");
        $display("  [2] BEQ x1, x2, +0           → x1==x2, BRANCH TAKEN to PC=12");
        $display("  [3] ADDI x3, x0, 12          ← Wrong path (skipped)");
        $display("  [4] ADDI x3, x0, 12          ← Wrong path (skipped)");
        $display("  [5] ADD x4, x1, x2           → Target: x4 = 5 + 5 = 10");
        $display("  [6] ADD x5, x4, x4           → x5 = 10 + 10 = 20");
        $display("\nExpected: x1=5, x2=5, x3=0 (never written), x4=10, x5=20\n");
    endtask

    task load_program_branch_not_taken();
        $display("\n=== Program: Branch Not Taken ===\n");
        
        // Program: x1=5, x2=10, BEQ (not taken), normal path continues
        dut.instr_mem[0]  = 32'b000000000101_00000_000_00001_0010011; // ADDI x1, x0, 5
        dut.instr_mem[1]  = 32'b000000001010_00000_000_00010_0010011; // ADDI x2, x0, 10
        dut.instr_mem[2]  = 32'b0000000_00010_00001_000_00000_1100011; // BEQ x1, x2, +0 (NOT taken — offset irrelevant)
        dut.instr_mem[5]  = 32'd0; // Clear stale instr from Test 1
        dut.instr_mem[6]  = 32'd0; // Clear stale instr from Test 1
        dut.instr_mem[3]  = 32'b0000000_00010_00001_000_00011_0110011; // ADD x3, x1, x2
        dut.instr_mem[4]  = 32'b0000000_00011_00011_000_00100_0110011; // ADD x4, x3, x3
        
        $display("Program:");
        $display("  [0] ADDI x1, x0, 5           → x1 = 5");
        $display("  [1] ADDI x2, x0, 10          → x2 = 10");
        $display("  [2] BEQ x1, x2, +0           → x1!=x2, NOT TAKEN (no penalty)");
        $display("  [3] ADD x3, x1, x2           → x3 = 5 + 10 = 15");
        $display("  [4] ADD x4, x3, x3           → x4 = 15 + 15 = 30");
        $display("\nExpected: x1=5, x2=10, x3=15, x4=30 (continuous pipeline)\n");
    endtask

    task load_program_loop();
        $display("\n=== Program: Loop with Backward Branch (BNE) ===\n");
        
        // Simple accumulator loop: sum 1+2+3
        dut.instr_mem[0]  = 32'b000000000011_00000_000_00001_0010011; // ADDI x1, x0, 3         (counter)
        dut.instr_mem[1]  = 32'b000000000000_00000_000_00010_0010011; // ADDI x2, x0, 0         (sum)
        dut.instr_mem[2]  = 32'b0000000_00001_00010_000_00010_0110011; // ADD x2, x2, x1         (loop body: sum += counter)
        dut.instr_mem[3]  = 32'b111111111111_00001_000_00001_0010011; // ADDI x1, x1, -1        (counter--)
        dut.instr_mem[4]  = 32'b1111111_00000_00001_001_11001_1100011; // BNE x1, x0, -8         (loop back if x1 != 0)
        dut.instr_mem[5]  = 32'b0000000_00010_00000_000_00011_0110011; // ADD x3, x0, x2         (result)
        
        $display("Program (Loop: 3 + 2 + 1):");
        $display("  [0] ADDI x1, x0, 3           → x1 = 3 (counter)");
        $display("  [1] ADDI x2, x0, 0           → x2 = 0 (accumulator)");
        $display("  [2] ADD x2, x2, x1           → LOOP: x2 += x1");
        $display("  [3] ADDI x1, x1, -1          → x1--");
        $display("  [4] BNE x1, x0, -8           → if x1!=0, branch back to [2]");
        $display("  [5] ADD x3, x0, x2           → x3 = x2 (result)");
        $display("\nIteration 1: x1=3, x2=0+3=3, x1=2, BNE taken");
        $display("Iteration 2: x2=3+2=5, x1=1, BNE taken");
        $display("Iteration 3: x2=5+1=6, x1=0, BNE not taken");
        $display("Result: x3 = 6");
        $display("\nExpected: x1=0, x2=6, x3=6\n");
    endtask

    task load_program_branch_with_forwarding();
        $display("\n=== Program: Branch with Forwarding (Inter-dependency) ===\n");
        
        // Program: Compute x1, immediately use in BEQ (requires forwarding)
        dut.instr_mem[0]  = 32'b0000000_00101_00100_000_00001_0110011; // ADD x1, x4, x5
        dut.instr_mem[1]  = 32'b000000000001_00000_000_00010_0010011; // ADDI x2, x0, 1
        dut.instr_mem[2]  = 32'b0000000_00010_00001_001_00000_1100011; // BNE x1, x2, +0 (branch decision requires x1 forwarded)
        dut.instr_mem[3]  = 32'b000000001111_00000_000_00011_0010011; // ADDI x3, x0, 15 (wrong path)
        dut.instr_mem[4]  = 32'b0000000_00001_00001_000_00100_0110011; // ADD x4, x1, x1 (target)
        
        dut.instr_mem[0] = 32'b0000000_00101_00100_000_00001_0110011; // ADD x1, x4, x5 (x1 = 5+3 = 8)
        dut.instr_mem[1] = 32'b000000001000_00000_000_00010_0010011; // ADDI x2, x0, 8 (x2 = 8)
        dut.instr_mem[2] = 32'b0000000_00010_00001_000_01000_1100011; // BEQ x1, x2, +8 (x1==x2, TAKEN → instr[4])
        dut.instr_mem[3] = 32'b000000001111_00000_000_00011_0010011; // ADDI x3, x0, 15 (wrong path, skipped)
        dut.instr_mem[4] = 32'b0000000_00001_00001_000_00100_0110011; // ADD x4, x1, x1 (target: x4 = 8+8 = 16)
        
        $display("Program:");
        $display("  [0] ADD x1, x4, x5           → x1 = 5 + 3 = 8");
        $display("  [1] ADDI x2, x0, 8           → x2 = 8");
        $display("  [2] BEQ x1, x2, +0           → x1==x2 (forwarded!), TAKEN");
        $display("  [3] ADDI x3, x0, 15          ← Skipped (wrong path)");
        $display("  [4] ADD x4, x1, x1           → Target: x4 = 8 + 8 = 16");
        $display("\nNote: BEQ uses x1 from ADD (EX-to-EX forwarding needed)");
        $display("Expected: x1=8, x2=8, x3=0 (never written), x4=16\n");
    endtask

    integer cycle_limit;
    
    initial begin
        rst = 1;
        dut.instr_mem[0] = 0;
        #10 rst = 0;
        
        //=========================================================
        // TEST 1: Forward Branch Taken
        //=========================================================
        load_program_forward_branch_taken();
        cycle = 0;
        cycle_limit = 30;
        
        $display("====== Running Cycles (Forward Branch) ======\n");
        while (cycle < cycle_limit) begin
            @(posedge clk);
            cycle++;
            #1;
            if (cycle <= 12) begin
                $display("Cycle %2d: PC=%h | if_id_instr=%h | id_branch=%b | id_ex_branch=%b | id_ex_mem_read=%b | br_taken=%b | flush_if_id=%b | flush_id_ex=%b | pc_we=%b | if_id_we=%b | bubble=%b",
                         cycle, dut.pc, dut.if_id_instr, dut.id_branch, dut.id_ex_branch, dut.id_ex_mem_read, dut.branch_taken, dut.flush_if_id, dut.flush_id_ex, dut.pc_write_enable, dut.if_id_write_enable, dut.insert_bubble);
            end
        end
        
        $display("\nTest 1 Results:");
        $display("x1 = %d (expected: 5)", dut.id_regfile.regs[1]);
        $display("x2 = %d (expected: 5)", dut.id_regfile.regs[2]);
        $display("x3 = %d (expected: 0, never written)", dut.id_regfile.regs[3]);
        $display("x4 = %d (expected: 10)", dut.id_regfile.regs[4]);
        $display("x5 = %d (expected: 20)", dut.id_regfile.regs[5]);
        
        if (dut.id_regfile.regs[1] == 5 && dut.id_regfile.regs[4] == 10)
            $display("✓ TEST 1 PASSED\n");
        else
            $display("✗ TEST 1 FAILED\n");
        
        //=========================================================
        // TEST 2: Branch Not Taken
        //=========================================================
        rst = 1;
        #10 rst = 0;
        load_program_branch_not_taken();
        cycle = 0;
        
        $display("\n====== Running Cycles (Branch Not Taken) ======\n");
        while (cycle < cycle_limit) begin
            @(posedge clk);
            cycle++;
            #1;
            if (cycle <= 10) begin
                $display("Cycle %2d: PC=%h | branch_taken=%b",
                         cycle, dut.pc, dut.branch_taken);
            end
        end
        
        $display("\nTest 2 Results:");
        $display("x1 = %d (expected: 5)", dut.id_regfile.regs[1]);
        $display("x2 = %d (expected: 10)", dut.id_regfile.regs[2]);
        $display("x3 = %d (expected: 15)", dut.id_regfile.regs[3]);
        $display("x4 = %d (expected: 30)", dut.id_regfile.regs[4]);
        
        if (dut.id_regfile.regs[3] == 15 && dut.id_regfile.regs[4] == 30)
            $display("✓ TEST 2 PASSED\n");
        else
            $display("✗ TEST 2 FAILED\n");
        
        //=========================================================
        // TEST 3: Loop (Backward Branch)
        //=========================================================
        rst = 1;
        #10 rst = 0;
        load_program_loop();
        cycle = 0;
        cycle_limit = 40;
        
        $display("\n====== Running Cycles (Loop) ======\n");
        while (cycle < cycle_limit) begin
            @(posedge clk);
            cycle++;
            #1;
            if ((dut.branch_taken || cycle < 15))
                $display("Cycle %2d: PC=%h | branch_taken=%b | x1=%d | x2=%d",
                         cycle, dut.pc, dut.branch_taken, dut.id_regfile.regs[1], dut.id_regfile.regs[2]);
        end
        
        $display("\nTest 3 Results:");
        $display("x1 = %d (expected: 0, counter done)", dut.id_regfile.regs[1]);
        $display("x2 = %d (expected: 6, accumulator final)", dut.id_regfile.regs[2]);
        $display("x3 = %d (expected: 6, result)", dut.id_regfile.regs[3]);
        
        if (dut.id_regfile.regs[2] == 6 && dut.id_regfile.regs[3] == 6)
            $display("✓ TEST 3 PASSED\n");
        else
            $display("✗ TEST 3 FAILED\n");
        
        //=========================================================
        // TEST 4: Branch with Forwarding
        //=========================================================
        rst = 1;
        #10 rst = 0;
        load_program_branch_with_forwarding();
        
        // Set initial register values for ADD input
        dut.id_regfile.regs[4] = 32'd5;  // x4 = 5
        dut.id_regfile.regs[5] = 32'd3;  // x5 = 3
        
        cycle = 0;
        
        $display("\n====== Running Cycles (Branch with Forwarding) ======\n");
        while (cycle < cycle_limit) begin
            @(posedge clk);
            cycle++;
            #1;
            if (cycle <= 12) begin
                $display("Cycle %2d: PC=%h | branch_taken=%b | forward_a=%b | forward_b=%b",
                         cycle, dut.pc, dut.branch_taken, dut.ex_forwarding.forward_a, dut.ex_forwarding.forward_b);
            end
        end
        
        $display("\nTest 4 Results:");
        $display("x1 = %d (expected: 8, from ADD)", dut.id_regfile.regs[1]);
        $display("x2 = %d (expected: 8)", dut.id_regfile.regs[2]);
        $display("x3 = %d (expected: 0, never written)", dut.id_regfile.regs[3]);
        $display("x4 = %d (expected: 16, target)", dut.id_regfile.regs[4]);
        
        if (dut.id_regfile.regs[1] == 8 && dut.id_regfile.regs[4] == 16)
            $display("✓ TEST 4 PASSED\n");
        else
            $display("✗ TEST 4 FAILED\n");
        
        $display("\n=== All Tests Complete ===\n");
        $finish;
    end

endmodule
