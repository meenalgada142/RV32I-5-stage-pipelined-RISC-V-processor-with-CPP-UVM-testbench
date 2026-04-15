// ALU Sequences
class alu_base_sequence extends uvm_sequence#(alu_transaction);
    `uvm_object_utils(alu_base_sequence)

    function new(string name = "alu_base_sequence");
        super.new(name);
    endfunction

    task body();
        alu_transaction tx;
        tx = alu_transaction::type_id::create("tx");
        start_item(tx);
        tx.a = 32'h0000000A;
        tx.b = 32'h00000005;
        tx.alu_op = 4'b0000; // ADD
        finish_item(tx);
    endtask
endclass

class alu_random_sequence extends uvm_sequence#(alu_transaction);
    `uvm_object_utils(alu_random_sequence)

    function new(string name = "alu_random_sequence");
        super.new(name);
    endfunction

    task body();
        alu_transaction tx;
        repeat(100) begin
            tx = alu_transaction::type_id::create("tx");
            start_item(tx);
            assert(tx.randomize());
            finish_item(tx);
        end
    endtask
endclass

class alu_corner_sequence extends uvm_sequence#(alu_transaction);
    `uvm_object_utils(alu_corner_sequence)

    function new(string name = "alu_corner_sequence");
        super.new(name);
    endfunction

    task body();
        alu_transaction tx;
        // Overflow cases
        tx = alu_transaction::type_id::create("tx");
        start_item(tx);
        tx.a = 32'hFFFFFFFF;
        tx.b = 32'h00000001;
        tx.alu_op = 4'b0000; // ADD overflow
        finish_item(tx);

        tx = alu_transaction::type_id::create("tx");
        start_item(tx);
        tx.a = 32'h00000000;
        tx.b = 32'hFFFFFFFF;
        tx.alu_op = 4'b0001; // SUB underflow
        finish_item(tx);

        // Shift edge cases
        tx = alu_transaction::type_id::create("tx");
        start_item(tx);
        tx.a = 32'hAAAAAAAA;
        tx.b = 32'h00000020; // Shift by 32
        tx.alu_op = 4'b0110; // SLL
        finish_item(tx);
    endtask
endclass