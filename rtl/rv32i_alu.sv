module rv32i_alu (
    input  logic [31:0] a,
    input  logic [31:0] b,
    input  logic [3:0]  alu_op,
    output logic [31:0] result,
    output logic        zero
);

    logic [31:0] add_r;
    logic [31:0] sub_r;
    logic [31:0] and_r;
    logic [31:0] or_r;
    logic [31:0] xor_r;
    logic [31:0] sll_r;
    logic [31:0] srl_r;
    logic [31:0] sra_r;
    logic        slt_r;
    logic        sltu_r;

    assign add_r  = a + b;
    assign sub_r  = a - b;
    assign and_r  = a & b;
    assign or_r   = a | b;
    assign xor_r  = a ^ b;
    assign sll_r  = a << b[4:0];
    assign srl_r  = a >> b[4:0];            // logical right shift
    assign sra_r  = $signed(a) >>> b[4:0];  // arithmetic right shift
    assign slt_r  = ($signed(a) < $signed(b));   // signed comparison
    assign sltu_r = (a < b);                // unsigned comparison

    always @(*) begin
        unique case (alu_op)
            4'b0000: result = add_r;               // ADD
            4'b0001: result = sub_r;               // SUB
            4'b0010: result = and_r;               // AND
            4'b0011: result = or_r;                // OR
            4'b0100: result = xor_r;               // XOR
            4'b0101: result = {31'b0, slt_r};      // SLT (signed)
            4'b0110: result = sll_r;               // SLL
            4'b0111: result = srl_r;               // SRL (logical)
            4'b1000: result = {31'b0, sltu_r};     // SLTU (unsigned)
            4'b1001: result = sra_r;               // SRA (arithmetic)
            default: result = 32'h0000_0000;
        endcase
    end

    assign zero = (result == 32'h0000_0000);

endmodule
