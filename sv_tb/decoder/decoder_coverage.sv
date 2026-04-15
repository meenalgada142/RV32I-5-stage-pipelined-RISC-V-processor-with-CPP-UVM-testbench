// Decoder Coverage
class decoder_coverage extends uvm_subscriber#(decoder_transaction);
    `uvm_component_utils(decoder_coverage)

    decoder_transaction tx;

    covergroup decoder_cg;
        opcode_cp: coverpoint tx.instr[6:0] {
            bins r_type = {7'b0110011};
            bins i_type = {7'b0010011};
            bins load = {7'b0000011};
            bins store = {7'b0100011};
            bins branch = {7'b1100011};
            bins jal = {7'b1101111};
        }
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
        control_signals_cp: coverpoint {tx.reg_write, tx.mem_read, tx.mem_write, tx.branch, tx.jump};
        cross opcode_cp, control_signals_cp;
    endgroup

    function new(string name, uvm_component parent);
        super.new(name, parent);
        decoder_cg = new();
    endfunction

    function void write(decoder_transaction t);
        tx = t;
        decoder_cg.sample();
    endfunction
endclass