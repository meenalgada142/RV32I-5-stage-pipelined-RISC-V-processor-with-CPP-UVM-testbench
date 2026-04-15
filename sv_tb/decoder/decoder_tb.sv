// Decoder Top Testbench
module decoder_tb;
    import uvm_pkg::*;
    `include "uvm_macros.svh"

    logic clk;
    logic rst;

    decoder_if vif(clk, rst);

    rv32i_decoder dut (
        .instr(vif.instr),
        .rs1(vif.rs1),
        .rs2(vif.rs2),
        .rd(vif.rd),
        .imm(vif.imm),
        .alu_op(vif.alu_op),
        .reg_write(vif.reg_write),
        .mem_read(vif.mem_read),
        .mem_write(vif.mem_write),
        .mem_to_reg(vif.mem_to_reg),
        .alu_src(vif.alu_src),
        .branch(vif.branch),
        .branch_type(vif.branch_type),
        .jump(vif.jump)
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
        uvm_config_db#(virtual decoder_if.DRV)::set(null, "uvm_test_top.env.agt.drv", "vif", vif.DRV);
        uvm_config_db#(virtual decoder_if.MON)::set(null, "uvm_test_top.env.agt.mon", "vif", vif.MON);
        run_test();
    end
endmodule