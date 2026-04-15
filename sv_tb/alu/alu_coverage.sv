// ALU Coverage
class alu_coverage extends uvm_subscriber#(alu_transaction);
    `uvm_component_utils(alu_coverage)

    alu_transaction tx;

    covergroup alu_cg;
        alu_op_cp: coverpoint tx.alu_op {
            bins add = {4'b0000};
            bins sub = {4'b0001};
            bins and_op = {4'b0010};
            bins or_op = {4'b0011};
            bins xor_op = {4'b0100};
            bins slt = {4'b0101};
            bins sll = {4'b0110};
            bins srl = {4'b0111};
            bins sltu = {4'b1000};
            bins sra = {4'b1001};
        }
        a_cp: coverpoint tx.a {
            bins zero = {32'h00000000};
            bins max = {32'hFFFFFFFF};
            bins others = default;
        }
        b_cp: coverpoint tx.b {
            bins zero = {32'h00000000};
            bins max = {32'hFFFFFFFF};
            bins others = default;
        }
        zero_cp: coverpoint tx.zero;
        cross alu_op_cp, zero_cp;
    endgroup

    function new(string name, uvm_component parent);
        super.new(name, parent);
        alu_cg = new();
    endfunction

    function void write(alu_transaction t);
        tx = t;
        alu_cg.sample();
    endfunction
endclass