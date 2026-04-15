// Decoder Transaction
class decoder_transaction extends uvm_sequence_item;
    `uvm_object_utils(decoder_transaction)

    rand logic [31:0] instr;
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

    // Constraints for valid instructions
    constraint valid_instr {
        instr[1:0] == 2'b11; // RV32I instructions
        instr[6:0] inside {7'b0110011, 7'b0010011, 7'b0000011, 7'b0100011, 7'b1100011, 7'b1101111};
    }

    function new(string name = "decoder_transaction");
        super.new(name);
    endfunction

    function void do_print(uvm_printer printer);
        super.do_print(printer);
        printer.print_field("instr", instr, 32, UVM_HEX);
        printer.print_field("rs1", rs1, 5, UVM_DEC);
        printer.print_field("rs2", rs2, 5, UVM_DEC);
        printer.print_field("rd", rd, 5, UVM_DEC);
        printer.print_field("imm", imm, 32, UVM_HEX);
        printer.print_field("alu_op", alu_op, 4, UVM_DEC);
        printer.print_field("reg_write", reg_write, 1, UVM_DEC);
        printer.print_field("mem_read", mem_read, 1, UVM_DEC);
        printer.print_field("mem_write", mem_write, 1, UVM_DEC);
        printer.print_field("mem_to_reg", mem_to_reg, 1, UVM_DEC);
        printer.print_field("alu_src", alu_src, 1, UVM_DEC);
        printer.print_field("branch", branch, 1, UVM_DEC);
        printer.print_field("branch_type", branch_type, 1, UVM_DEC);
        printer.print_field("jump", jump, 1, UVM_DEC);
    endfunction

    function bit do_compare(uvm_object rhs, uvm_comparer comparer);
        decoder_transaction rhs_;
        if (!$cast(rhs_, rhs)) return 0;
        return (instr == rhs_.instr && rs1 == rhs_.rs1 && rs2 == rhs_.rs2 && rd == rhs_.rd &&
                imm == rhs_.imm && alu_op == rhs_.alu_op && reg_write == rhs_.reg_write &&
                mem_read == rhs_.mem_read && mem_write == rhs_.mem_write && mem_to_reg == rhs_.mem_to_reg &&
                alu_src == rhs_.alu_src && branch == rhs_.branch && branch_type == rhs_.branch_type &&
                jump == rhs_.jump);
    endfunction

    function void do_copy(uvm_object rhs);
        decoder_transaction rhs_;
        if (!$cast(rhs_, rhs)) return;
        super.do_copy(rhs);
        instr = rhs_.instr;
        rs1 = rhs_.rs1;
        rs2 = rhs_.rs2;
        rd = rhs_.rd;
        imm = rhs_.imm;
        alu_op = rhs_.alu_op;
        reg_write = rhs_.reg_write;
        mem_read = rhs_.mem_read;
        mem_write = rhs_.mem_write;
        mem_to_reg = rhs_.mem_to_reg;
        alu_src = rhs_.alu_src;
        branch = rhs_.branch;
        branch_type = rhs_.branch_type;
        jump = rhs_.jump;
    endfunction
endclass