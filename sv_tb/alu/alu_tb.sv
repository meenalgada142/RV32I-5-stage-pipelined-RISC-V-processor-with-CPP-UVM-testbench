// ALU Top Testbench
module alu_tb;
    import uvm_pkg::*;
    `include "uvm_macros.svh"

    logic clk;
    logic rst;

    alu_if vif(clk, rst);

    rv32i_alu dut (
        .a(vif.a),
        .b(vif.b),
        .alu_op(vif.alu_op),
        .result(vif.result),
        .zero(vif.zero)
    );

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        rst = 1;
        #10 rst = 0;
    end

    initial begin
        uvm_config_db#(virtual alu_if.DRV)::set(null, "uvm_test_top.env.agt.drv", "vif", vif.DRV);
        uvm_config_db#(virtual alu_if.MON)::set(null, "uvm_test_top.env.agt.mon", "vif", vif.MON);
        run_test();
    end
endmodule