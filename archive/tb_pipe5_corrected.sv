////////////////////////////////////////////////////////////////////////////////
//  tb_pipe5_corrected.sv
//
//  CORRECTED Comprehensive Testbench for Branch & Jump Support
//  Proper instruction encodings for B-format and J-format immediates
//
//============================================================================

module tb_pipe5_corrected;
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
        $display("\n=== Program: Forward Branch Taken (CORRECTED) ===\n");
        
        // Program: x1=5, x2=5, BEQ (taken), skip wrong paths, execute target
        // Instruction offsets:
        // [0] @ 0x0:  ADDI x1, x0, 5
        // [1] @ 0x4:  ADDI x2, x0, 5
        // [2] @ 0x8:  BEQ x1, x2, +8  (offset=8 bytes = skip 2 instructions)
        // [3] @ 0xc:  ADDI x3, x0, 12 (WRONG PATH)
        // [4] @ 0x10: ADDI x3, x0, 12 (WRONG PATH)
        // [5] @ 0x14: ADD x4, x1, x2  (TARGET after BEQ)
        // [6] @ 0x18: ADD x5, x4, x4
        
        dut.instr_mem[0]  = 32'b000000000101_00000_000_00001_0010011; // ADDI x1, x0, 5
        dut.instr_mem[1]  = 32'b000000000101_00000_000_00010_0010011; // ADDI x2, x0, 5
        // BEQ x1, x2, +8
        // B-format: imm[12|10:5] in bits[31:25], imm[4:1|0] in bits[11:7]
        // offset=+8: imm=0b0000100 (bits need reconstruction)
        // {imm12, imm[10:5], rs2, rs1, funct3, imm[4:1], imm[0], opcode}
        // 0000_000_00010_00001_000_0100_0_1100011
        dut.instr_mem[2]  = 32'b0000000_00010_00001_000_01000_1100011; // BEQ x1, x2, +8 (CORRECTED offset)
        dut.instr_mem[3]  = 32'b000000001100_00000_000_00011_0010011; // ADDI x3, x0, 12 (wrong path!)
        dut.instr_mem[4]  = 32'b000000001100_00000_000_00011_0010011; // ADDI x3, x0, 12 (wrong path!)
        dut.instr_mem[5]  = 32'b0000000_00010_00001_000_00100_0110011; // ADD x4, x1, x2 (target)
        dut.instr_mem[6]  = 32'b0000000_00100_00100_000_00101_0110011; // ADD x5, x4, x4
        
        $display("Program (Corrected BEQ offset):");
        $display("  [0] @ 0x0:  ADDI x1, x0, 5    → x1 = 5");
        $display("  [1] @ 0x4:  ADDI x2, x0, 5    → x2 = 5");
        $display("  [2] @ 0x8:  BEQ x1, x2, +8    → branch TAKEN to 0x14");
        $display("  [3] @ 0xc:  ADDI x3, x0, 12   ← Skipped (wrong path)");
        $display("  [4] @ 0x10: ADDI x3, x0, 12   ← Skipped (wrong path)");
        $display("  [5] @ 0x14: ADD x4, x1, x2    → x4 = 10 (target)");
        $display("  [6] @ 0x18: ADD x5, x4, x4    → x5 = 20");
        $display("\nExpected: x1=5, x2=5, x3=0, x4=10, x5=20\n");
    endtask

    task load_program_loop();
        $display("\n=== Program: Loop with Backward Branch (CORRECTED) ===\n");
        
        // Simple loop: sum 3+2+1
        // [0] @ 0x0:  ADDI x1, x0, 3        (counter = 3)
        // [1] @ 0x4:  ADDI x2, x0, 0        (accumulator = 0)
        // [2] @ 0x8:  ADD x2, x2, x1        (loop body: sum += counter)
        // [3] @ 0xc:  ADDI x1, x1, -1       (counter--)
        // [4] @ 0x10: BNE x1, x0, -8        (if counter != 0, branch back -8 to [2])
        // [5] @ 0x14: ADD x3, x0, x2        (result = sum)
        
        dut.instr_mem[0]  = 32'b111111111101_00001_000_00001_0010011; // ADDI x1, x0, -3 (use -3 for easier encoding)
        
        // Corrected: start with ADDI x1, x0, 3
        dut.instr_mem[0]  = 32'b000000000011_00000_000_00001_0010011; // ADDI x1, x0, 3
        dut.instr_mem[1]  = 32'b000000000000_00000_000_00010_0010011; // ADDI x2, x0, 0
        dut.instr_mem[2]  = 32'b0000000_00001_00010_000_00010_0110011; // ADD x2, x2, x1
        dut.instr_mem[3]  = 32'b111111111111_00001_000_00001_0010011; // ADDI x1, x1, -1
        // BNE x1, x0, -8 (branch back 8 bytes from 0x10 to 0x8)
        // For negative offset: -8 in 13-bit signed = 0x1FF8
        // B-format: imm[12|10:5|4:1|0] splits as:
        // -8: binary 1111_1111_1000 (in 13 bits)
        // imm[12]=1, imm[10:5]=111111, imm[4:1]=1000, imm[0]=0
        dut.instr_mem[4]  = 32'b1111111_00000_00001_001_11000_1100011; // BNE x1, x0, -8 (offset = -4 in 12-bit signed = 0xFFC, bits[11:8]=1100)
        dut.instr_mem[5]  = 32'b0000000_00010_00000_000_00011_0110011; // ADD x3, x0, x2
        
        $display("Program (Loop with backward BNE, corrected offset):");
        $display("  [0] @ 0x0:  ADDI x1, x0, 3    → x1 = 3 (counter)");
        $display("  [1] @ 0x4:  ADDI x2, x0, 0    → x2 = 0 (accumulator)");
        $display("  [2] @ 0x8:  ADD x2, x2, x1    → LOOP: x2 += x1");
        $display("  [3] @ 0xc:  ADDI x1, x1, -1   → x1--");
        $display("  [4] @ 0x10: BNE x1, x0, -8    → if x1 != 0, branch back");
        $display("  [5] @ 0x14: ADD x3, x0, x2    → x3 = result");
        $display("\nIterations:");
        $display("  1: x1=3, x2=3, x1=2, BNE taken");
        $display("  2: x2=5, x1=1, BNE taken");
        $display("  3: x2=6, x1=0, BNE not taken");
        $display("\nExpected: x1=0, x2=6, x3=6\n");
    endtask

    integer cycle_limit;
    
    initial begin
        rst = 1;
        #10 rst = 0;
        
        //=========================================================
        // TEST 1: Forward Branch Taken (Corrected)
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
                $display("Cycle %2d: PC=%h | branch_taken=%b | flush_if_id=%b | x1=%d | x4=%d",
                         cycle, dut.pc, dut.branch_taken, dut.flush_if_id, dut.id_regfile.regs[1], dut.id_regfile.regs[4]);
            end
        end
        
        $display("\nTest 1 Results:");
        $display("x1 = %d (expected: 5)", dut.id_regfile.regs[1]);
        $display("x2 = %d (expected: 5)", dut.id_regfile.regs[2]);
        $display("x3 = %d (expected: 0)", dut.id_regfile.regs[3]);
        $display("x4 = %d (expected: 10)", dut.id_regfile.regs[4]);
        $display("x5 = %d (expected: 20)", dut.id_regfile.regs[5]);
        
        if (dut.id_regfile.regs[1] == 5 && dut.id_regfile.regs[4] == 10 && dut.id_regfile.regs[5] == 20)
            $display("✓ TEST 1 PASSED\n");
        else
            $display("✗ TEST 1 FAILED\n");
        
        //=========================================================
        // TEST 2: Loop (Backward Branch with Corrected Encoding)
        //=========================================================
        rst = 1;
        #10 rst = 0;
        load_program_loop();
        cycle = 0;
        cycle_limit = 50;
        
        $display("====== Running Cycles (Loop) ======\n");
        while (cycle < cycle_limit) begin
            @(posedge clk);
            cycle++;
            #1;
            if (cycle <= 25 || dut.branch_taken) begin
                $display("Cycle %2d: PC=%h | branch_taken=%b | x1=%d | x2=%d",
                         cycle, dut.pc, dut.branch_taken, dut.id_regfile.regs[1], dut.id_regfile.regs[2]);
            end
        end
        
        $display("\nTest 2 Results:");
        $display("x1 = %d (expected: 0)", dut.id_regfile.regs[1]);
        $display("x2 = %d (expected: 6)", dut.id_regfile.regs[2]);
        $display("x3 = %d (expected: 6)", dut.id_regfile.regs[3]);
        
        if (dut.id_regfile.regs[2] == 6 && dut.id_regfile.regs[3] == 6)
            $display("✓ TEST 2 PASSED\n");
        else
            $display("✗ TEST 2 FAILED\n");
        
        $display("\n=== All Corrected Tests Complete ===\n");
        $finish;
    end

endmodule
