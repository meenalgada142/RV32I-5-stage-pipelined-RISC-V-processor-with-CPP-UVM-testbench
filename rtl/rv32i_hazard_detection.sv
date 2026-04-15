//============================================================================
// rv32i_hazard_detection.sv
// Hazard detection and stall unit for 5-stage RISC-V pipeline
//============================================================================

module rv32i_hazard_detection (
    // ID stage (current instruction trying to read)
    input  logic [4:0]  id_rs1,
    input  logic [4:0]  id_rs2,
    
    // EX stage (load instruction result not yet available)
    input  logic [4:0]  ex_rd,
    input  logic        ex_memread,
    
    // Control outputs
    output logic        stall,
    output logic        pc_write_enable,
    output logic        if_id_write_enable,
    output logic        insert_bubble
);

    //========================================================================
    // Load-Use Hazard Detection
    //========================================================================
    // Hazard occurs when:
    // 1. EX stage is performing a load (ex_memread == 1)
    // 2. EX.rd matches ID.rs1 or ID.rs2 (and rd != 0)
    //========================================================================
    
    logic load_use_hazard;
    
    always @(*) begin
        // Detect load-use hazard
        if (ex_memread && (ex_rd != 5'd0)) begin
            if ((ex_rd == id_rs1) || (ex_rd == id_rs2)) begin
                load_use_hazard = 1'b1;
            end else begin
                load_use_hazard = 1'b0;
            end
        end else begin
            load_use_hazard = 1'b0;
        end
    end
    
    //========================================================================
    // Stall Control Signals
    //========================================================================
    assign stall             = load_use_hazard;
    assign pc_write_enable   = ~load_use_hazard;   // Freeze PC if stalling
    assign if_id_write_enable = ~load_use_hazard;  // Freeze IF/ID register
    assign insert_bubble     = load_use_hazard;    // Insert NOP in ID/EX

endmodule
