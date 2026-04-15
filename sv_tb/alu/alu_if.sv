// ALU Interface
interface alu_if(input logic clk, input logic rst);
    logic [31:0] a;
    logic [31:0] b;
    logic [3:0]  alu_op;
    logic [31:0] result;
    logic        zero;

    // Clocking block for driver
    clocking drv_cb @(posedge clk);
        output a, b, alu_op;
        input result, zero;
    endclocking

    // Clocking block for monitor
    clocking mon_cb @(posedge clk);
        input a, b, alu_op, result, zero;
    endclocking

    // Modports
    modport DRV (clocking drv_cb, input rst);
    modport MON (clocking mon_cb, input rst);
endinterface