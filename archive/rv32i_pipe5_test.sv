//============================================================================
// rv32i_pipe5_test.sv
// Advanced testbench with program loading for 5-stage pipeline
//============================================================================

module rv32i_pipe5_test;
    logic clk;
    logic rst;
    logic [31:0] alu_result;
    logic zero;

    // DUT
    rv32i_pipe5 dut (
        .clk(clk),
        .rst(rst),
        .alu_result(alu_result),
        .zero(zero)
    );

    // Clock generation
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    // Program loading task
    task load_program();
        // Load test program into instruction memory
        // ADDI x1, x0, 5       instr[0]
        dut.instr_mem[0] = 32'b000000000101_00000_000_00001_0010011;
        
        // ADDI x2, x0, 3       instr[1]
        dut.instr_mem[1] = 32'b000000000011_00000_000_00010_0010011;
        
        // ADD x3, x1, x2       instr[2]
        dut.instr_mem[2] = 32'b0000000_00010_00001_000_00011_0110011;
        
        // SUB x4, x1, x2       instr[3]
        dut.instr_mem[3] = 32'b0100000_00010_00001_000_00100_0110011;
        
        // AND x5, x1, x2       instr[4]
        dut.instr_mem[4] = 32'b0000000_00010_00001_111_00101_0110011;
        
        // OR x6, x1, x2        instr[5]
        dut.instr_mem[5] = 32'b0000000_00010_00001_110_00110_0110011;
        
        // XOR x7, x1, x2       instr[6]
        dut.instr_mem[6] = 32'b0000000_00010_00001_100_00111_0110011;
        
        // Initialize data memory with test values
        dut.data_mem[0] = 32'hDEAD_BEEF;
        dut.data_mem[1] = 32'hCAFE_BABE;
        
        $display("Program loaded:");
        $display("  [0] ADDI x1, x0, 5");
        $display("  [1] ADDI x2, x0, 3");
        $display("  [2] ADD x3, x1, x2");
        $display("  [3] SUB x4, x1, x2");
        $display("  [4] AND x5, x1, x2");
        $display("  [5] OR x6, x1, x2");
        $display("  [6] XOR x7, x1, x2");
    endtask

    // Test sequence
    initial begin
        load_program();
        
        rst = 1;
        #10 rst = 0;
        
        $display("\n=== Pipeline Execution Starts ===\n");
        
        // Run for enough cycles to complete all instructions
        repeat(20) begin
            @(posedge clk);
            #1;
            $display("Cycle: PC=%h | ALU_Result=%h | zero=%b", 
                     dut.pc, alu_result, zero);
        end
        
        $display("\n=== Final Register State ===");
        $display("x1 = %h (expected: 5)", dut.id_regfile.regs[1]);
        $display("x2 = %h (expected: 3)", dut.id_regfile.regs[2]);
        $display("x3 = %h (expected: 8, ADD result)", dut.id_regfile.regs[3]);
        $display("x4 = %h (expected: 2, SUB result)", dut.id_regfile.regs[4]);
        $display("x5 = %h (expected: 1, AND result)", dut.id_regfile.regs[5]);
        $display("x6 = %h (expected: 7, OR result)", dut.id_regfile.regs[6]);
        $display("x7 = %h (expected: 6, XOR result)", dut.id_regfile.regs[7]);
        
        $finish;
    end

endmodule
