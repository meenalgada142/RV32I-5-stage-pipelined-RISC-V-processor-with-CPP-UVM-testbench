// Decoder Interface
interface decoder_if(input logic clk, input logic rst);
    logic [31:0] instr;
    logic [4:0]  rs1;
    logic [4:0]  rs2;
    logic [4:0]  rd;
    logic [31:0] imm;
    logic [3:0]  alu_op;
    logic        reg_write;
    logic        mem_read;
    logic        mem_write;
    logic        mem_to_reg;
    logic        alu_src;
    logic        branch;
    logic        branch_type;
    logic        jump;

    // Clocking block for driver
    clocking drv_cb @(posedge clk);
        output instr;
        input rs1, rs2, rd, imm, alu_op, reg_write, mem_read, mem_write, mem_to_reg, alu_src, branch, branch_type, jump;
    endclocking

    // Clocking block for monitor
    clocking mon_cb @(posedge clk);
        input instr, rs1, rs2, rd, imm, alu_op, reg_write, mem_read, mem_write, mem_to_reg, alu_src, branch, branch_type, jump;
    endclocking

    // Modports
    modport DRV (clocking drv_cb, input rst);
    modport MON (clocking mon_cb, input rst);
endinterface