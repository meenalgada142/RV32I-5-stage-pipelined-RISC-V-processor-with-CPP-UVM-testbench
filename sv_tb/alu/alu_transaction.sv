// ALU Transaction
class alu_transaction extends uvm_sequence_item;
    `uvm_object_utils(alu_transaction)

    rand logic [31:0] a;
    rand logic [31:0] b;
    rand logic [3:0]  alu_op;
    logic [31:0] result;
    logic        zero;

    // Constraints
    constraint valid_alu_op {
        alu_op inside {[0:9]};
    }

    constraint edge_values {
        a dist {32'h00000000:/10, 32'hFFFFFFFF:/10, [32'h00000001:32'hFFFFFFFE]:/80};
        b dist {32'h00000000:/10, 32'hFFFFFFFF:/10, [32'h00000001:32'hFFFFFFFE]:/80};
    }

    function new(string name = "alu_transaction");
        super.new(name);
    endfunction

    function void do_print(uvm_printer printer);
        super.do_print(printer);
        printer.print_field("a", a, 32, UVM_HEX);
        printer.print_field("b", b, 32, UVM_HEX);
        printer.print_field("alu_op", alu_op, 4, UVM_DEC);
        printer.print_field("result", result, 32, UVM_HEX);
        printer.print_field("zero", zero, 1, UVM_DEC);
    endfunction

    function bit do_compare(uvm_object rhs, uvm_comparer comparer);
        alu_transaction rhs_;
        if (!$cast(rhs_, rhs)) return 0;
        return (a == rhs_.a && b == rhs_.b && alu_op == rhs_.alu_op &&
                result == rhs_.result && zero == rhs_.zero);
    endfunction

    function void do_copy(uvm_object rhs);
        alu_transaction rhs_;
        if (!$cast(rhs_, rhs)) return;
        super.do_copy(rhs);
        a = rhs_.a;
        b = rhs_.b;
        alu_op = rhs_.alu_op;
        result = rhs_.result;
        zero = rhs_.zero;
    endfunction
endclass