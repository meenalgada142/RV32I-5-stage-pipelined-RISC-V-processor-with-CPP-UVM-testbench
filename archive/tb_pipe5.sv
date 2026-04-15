//============================================================================
// tb_pipe5.sv
// Testbench for 5-stage pipelined RISC-V processor
//============================================================================

module tb_pipe5;
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

    // Test sequence
    initial begin
        // Initialize instruction memory inside DUT (would need to make it public)
        // For now, we'll rely on the module's local memory
        
        rst = 1;
        #10 rst = 0;

        // Allow pipeline to fill
        // Cycle 1: IF fetches instr[0]
        #10;

        // Cycle 2: ID decodes instr[0], IF fetches instr[1]
        #10;

        // Cycle 3: EX executes instr[0], ID decodes instr[1], IF fetches instr[2]
        #10;

        // Cycle 4: MEM for instr[0], EX for instr[1], ID for instr[2], IF for instr[3]
        #10;

        // Cycle 5: WB for instr[0], MEM for instr[1], EX for instr[2], ID for instr[3]
        #10;

        // Continue for several more cycles to see full pipeline operation
        repeat(10) #10;

        $finish;
    end

    // Monitor pipeline stages
    initial begin
        $monitor("Time=%0t | clk=%b | PC=%h | ALU=%h | zero=%b",
                 $time, clk, dut.pc, alu_result, zero);
    end

endmodule
