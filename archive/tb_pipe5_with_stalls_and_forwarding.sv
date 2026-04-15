//============================================================================
// tb_pipe5_with_stalls_and_forwarding.sv
// Testbench demonstrating stall behavior for load-use hazards
//============================================================================

module tb_pipe5_with_stalls_and_forwarding;
    logic clk;
    logic rst;
    logic [31:0] alu_result;
    logic zero;

    rv32i_pipe5_with_forwarding_stalls dut (
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

    task load_program_load_use_hazard();
        $display("\n=== Program: Load-Use Hazard Test ===\n");
        
        // Load followed by dependent instructions
        dut.instr_mem[0] = 32'b000000001000_00000_010_00001_0000011; // LW x1, 8(x0)
        dut.instr_mem[1] = 32'b0000000_00001_00000_000_00010_0110011; // ADD x2, x0, x1  ← STALL
        dut.instr_mem[2] = 32'b000000000011_00000_000_00011_0010011; // ADDI x3, x0, 3
        dut.instr_mem[3] = 32'b0000000_00001_00001_000_00100_0110011; // ADD x4, x1, x1  ← Uses x1
        dut.instr_mem[4] = 32'b0000000_00010_00011_000_00101_0110011; // ADD x5, x3, x2
        
        // Initialize data memory
        dut.data_mem[2] = 32'h0000_00AA;  // Will be read by LW x1, 8(x0)
        
        $display("Program:");
        $display("  [0] LW x1, 8(x0)              ← Loads 0xAA from memory");
        $display("  [1] ADD x2, x0, x1            ← STALLS waiting for x1");
        $display("  [2] ADDI x3, x0, 3            ← Independent (no stall)");
        $display("  [3] ADD x4, x1, x1            ← Uses x1");
        $display("  [4] ADD x5, x3, x2            ← Uses x2 from [1]");
        $display("\nExpected Timeline:\n");
        $display("  Cycle 1: LW(EX), prev(MEM), prev(WB)");
        $display("  Cycle 2: LW(MEM), ADD(ID←STALLED), prev(WB)");
        $display("  Cycle 3: LW(WB), ADD(EX), ADDI(ID)");
        $display("  Cycle 4: next, next, ADD(EX)\n");
    endtask

    task load_program_simple_independent();
        $display("\n=== Program: Independent Instructions (No Stalls) ===\n");
        
        dut.instr_mem[0] = 32'b000000000101_00000_000_00001_0010011; // ADDI x1, x0, 5
        dut.instr_mem[1] = 32'b000000000011_00000_000_00010_0010011; // ADDI x2, x0, 3
        dut.instr_mem[2] = 32'b0000000_00010_00001_000_00011_0110011; // ADD x3, x1, x2
        dut.instr_mem[3] = 32'b0000000_00010_00001_111_00100_0110011; // AND x4, x1, x2
        dut.instr_mem[4] = 32'b0000000_00010_00001_110_00101_0110011; // OR x5, x1, x2
        
        $display("Program (all independent ADDI/ALU):");
        $display("  [0] ADDI x1, x0, 5");
        $display("  [1] ADDI x2, x0, 3");
        $display("  [2] ADD x3, x1, x2 (forwarded x1, x2)");
        $display("  [3] AND x4, x1, x2");
        $display("  [4] OR x5, x1, x2\n");
        $display("Expected: NO STALLS (forwarding handles all dependencies)\n");
    endtask

    initial begin
        rst = 1;
        #10 rst = 0;
        
        // TEST 1: Independent instructions (no stalls)
        load_program_simple_independent();
        $display("====== Running Cycles ======\n");
        
        repeat(15) begin
            @(posedge clk);
            cycle++;
            #1;
            $display("Cycle %2d: PC=%h | ALU=%h | stall=%b | pc_wr=%b | if_id_wr=%b | bubble=%b",
                     cycle,
                     dut.pc,
                     alu_result,
                     dut.hz_detect.stall,
                     dut.pc_write_enable,
                     dut.if_id_write_enable,
                     dut.insert_bubble);
        end
        
        $display("\nFirst Test Results:");
        $display("x1 = %d (expected: 5)", dut.id_regfile.regs[1]);
        $display("x2 = %d (expected: 3)", dut.id_regfile.regs[2]);
        $display("x3 = %d (expected: 8, 5+3)", dut.id_regfile.regs[3]);
        $display("x4 = %d (expected: 1, 5&3)", dut.id_regfile.regs[4]);
        $display("x5 = %d (expected: 7, 5|3)", dut.id_regfile.regs[5]);
        
        // TEST 2: Load-use hazard (with stalls)
        $display("\n\n");
        cycle = 0;
        rst = 1;
        #10 rst = 0;
        
        load_program_load_use_hazard();
        $display("====== Running Cycles (With Stalls) ======\n");
        
        repeat(20) begin
            @(posedge clk);
            cycle++;
            #1;
            
            // Show stall condition
            if (dut.hz_detect.stall) begin
                $display("Cycle %2d: PC=%h | STALLED | pc_wr=0 | if_id_wr=0 | bubble=1 ← LOAD-USE HAZARD",
                         cycle, dut.pc);
            end else begin
                $display("Cycle %2d: PC=%h | ALU=%h | running",
                         cycle, dut.pc, alu_result);
            end
        end
        
        $display("\n=== Final Register State ===");
        $display("x1 = %h (expected: 0xAA from LW)", dut.id_regfile.regs[1]);
        $display("x2 = %h (expected: 0xAA, x1 forwarded)", dut.id_regfile.regs[2]);
        $display("x3 = %h (expected: 3)", dut.id_regfile.regs[3]);
        $display("x4 = %h (expected: 0x154, 0xAA+0xAA)", dut.id_regfile.regs[4]);
        $display("x5 = %h (expected: 0xAD, 3+0xAA)", dut.id_regfile.regs[5]);
        
        $display("\n=== Verification ===");
        if (dut.id_regfile.regs[2] == 32'hAA)
            $display("✓ x2 = 0xAA (ADD x2, x0, x1 executed correctly)");
        else
            $display("✗ x2 != 0xAA (stall may not have worked)");
        
        if (dut.id_regfile.regs[4] == 32'h154)
            $display("✓ x4 = 0x154 (ADD x4, x1, x1 with x1=0xAA)");
        else
            $display("✗ x4 != 0x154");
        
        $display("\n=== Test Complete ===\n");
        $finish;
    end

endmodule
