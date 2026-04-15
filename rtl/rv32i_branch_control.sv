////////////////////////////////////////////////////////////////////////////////
//  rv32i_branch_control.sv
//  
//  Branch Control Unit for 5-Stage RISC-V Pipeline
//  
//  Determines branch decision, calculates target address, generates flush signals
//  
//  Purpose: Determine if a branch should be taken and route control signals
//  Stage: EX (branch decision occurs after ALU comparison)
//  
//  ISA: RV32I (BEQ, BNE instructions)
//==============================================================================

module rv32i_branch_control (
    // Instruction type and condition
    input  logic        branch,         // Is current EX instruction a branch?
    input  logic        branch_type,    // 0=BEQ, 1=BNE
    
    // Condition signals from ALU
    input  logic        zero,           // ALU zero flag (operands equal)
    
    // Address calculation
    input  logic [31:0] pc_ex,          // Current PC in EX stage
    input  logic [31:0] imm_ex,         // Immediate (sign-extended offset)
    input  logic [31:0] pc_normal,      // PC + 4 (normal increment)
    
    // Outputs
    output logic        branch_taken,   // Did branch execute?
    output logic [31:0] pc_next,        // Next PC value
    output logic        flush_if_id,    // Flush IF/ID register (insert NOP)
    output logic        flush_id_ex     // Flush ID/EX register (insert NOP)
);

    // Branch condition evaluation (combinational)
    logic condition_met;
    
    always_comb begin
        // Decode branch type and check condition
        case (branch_type)
            1'b0: condition_met = zero;        // BEQ: branch if equal (zero flag set)
            1'b1: condition_met = ~zero;       // BNE: branch if not equal (zero flag clear)
            default: condition_met = 1'b0;
        endcase
    end
    
    // Branch taken only if instruction is branch AND condition is met
    assign branch_taken = branch & condition_met;
    
    // PC multiplexer: choose branch target if taken, else normal increment
    assign pc_next = branch_taken ? (pc_ex + imm_ex) : pc_normal;
    
    // Flush signals: active when branch is taken
    // If branch_taken, we flush IF/ID and ID/EX to remove speculatively fetched instructions
    assign flush_if_id = branch_taken;
    assign flush_id_ex = branch_taken;

endmodule
