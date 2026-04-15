//============================================================================
// rv32i_forwarding_unit.sv
// Forwarding unit for 5-stage RISC-V pipeline data hazard resolution
//============================================================================

module rv32i_forwarding_unit (
    // From EX stage (request)
    input  logic [4:0]  ex_rs1,
    input  logic [4:0]  ex_rs2,
    
    // From MEM stage (result)
    input  logic [4:0]  mem_rd,
    input  logic        mem_regwrite,
    
    // From WB stage (result)
    input  logic [4:0]  wb_rd,
    input  logic        wb_regwrite,
    
    // Control outputs
    output logic [1:0]  forward_a,  // 00=regfile, 01=WB, 10=MEM
    output logic [1:0]  forward_b   // 00=regfile, 01=WB, 10=MEM
);

    //========================================================================
    // Forward A (first source operand: ex_rs1)
    //========================================================================
    always @(*) begin
        // Priority: MEM > WB > RegFile
        
        // Check MEM stage first (highest priority)
        if (mem_regwrite && (mem_rd == ex_rs1) && (mem_rd != 5'd0)) begin
            forward_a = 2'b10;  // Forward from MEM
        end
        // Check WB stage (lower priority)
        else if (wb_regwrite && (wb_rd == ex_rs1) && (wb_rd != 5'd0)) begin
            forward_a = 2'b01;  // Forward from WB
        end
        // No forwarding needed
        else begin
            forward_a = 2'b00;  // Use register file
        end
    end

    //========================================================================
    // Forward B (second source operand: ex_rs2)
    //========================================================================
    always @(*) begin
        // Priority: MEM > WB > RegFile
        
        // Check MEM stage first (highest priority)
        if (mem_regwrite && (mem_rd == ex_rs2) && (mem_rd != 5'd0)) begin
            forward_b = 2'b10;  // Forward from MEM
        end
        // Check WB stage (lower priority)
        else if (wb_regwrite && (wb_rd == ex_rs2) && (wb_rd != 5'd0)) begin
            forward_b = 2'b01;  // Forward from WB
        end
        // No forwarding needed
        else begin
            forward_b = 2'b00;  // Use register file
        end
    end

endmodule
