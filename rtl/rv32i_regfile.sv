module rv32i_regfile (
    input  logic        clk,
    input  logic        rst,
    input  logic [4:0]  rs1,
    input  logic [4:0]  rs2,
    input  logic [4:0]  rd,
    input  logic [31:0] wd,
    input  logic        write_enable,
    output logic [31:0] rd1,
    output logic [31:0] rd2
);

    logic [31:0] regs [31:0];

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            integer i;
            for (i = 0; i < 32; i = i + 1) begin
                regs[i] <= 32'd0;
            end
        end else begin
            if (write_enable && (rd != 5'd0)) begin
                regs[rd] <= wd;
            end
        end
    end

    // Write-first bypass: if WB is writing the same register being read this cycle,
    // return wd directly rather than the not-yet-updated regs[] entry.
    // Priority (outer to inner): x0 → bypass → stored value.
    always @(*) begin
        rd1 = (rs1 == 5'd0)                            ? 32'd0 :
              (write_enable && rd == rs1 && rd != 5'd0) ? wd    :
                                                          regs[rs1];

        rd2 = (rs2 == 5'd0)                            ? 32'd0 :
              (write_enable && rd == rs2 && rd != 5'd0) ? wd    :
                                                          regs[rs2];
    end

endmodule
