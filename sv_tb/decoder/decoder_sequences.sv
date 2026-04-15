// Decoder Sequences
class decoder_base_sequence extends uvm_sequence#(decoder_transaction);
    `uvm_object_utils(decoder_base_sequence)

    function new(string name = "decoder_base_sequence");
        super.new(name);
    endfunction

    task body();
        decoder_transaction tx;
        tx = decoder_transaction::type_id::create("tx");
        start_item(tx);
        tx.instr = 32'h00A58513; // ADDI x10, x11, 10
        finish_item(tx);
    endtask
endclass

class decoder_random_sequence extends uvm_sequence#(decoder_transaction);
    `uvm_object_utils(decoder_random_sequence)

    function new(string name = "decoder_random_sequence");
        super.new(name);
    endfunction

    task body();
        decoder_transaction tx;
        repeat(100) begin
            tx = decoder_transaction::type_id::create("tx");
            start_item(tx);
            assert(tx.randomize());
            finish_item(tx);
        end
    endtask
endclass

class decoder_corner_sequence extends uvm_sequence#(decoder_transaction);
    `uvm_object_utils(decoder_corner_sequence)

    function new(string name = "decoder_corner_sequence");
        super.new(name);
    endfunction

    task body();
        decoder_transaction tx;
        // R-type ADD
        tx = decoder_transaction::type_id::create("tx");
        start_item(tx);
        tx.instr = 32'h00B50533; // ADD x10, x10, x11
        finish_item(tx);

        // I-type LW
        tx = decoder_transaction::type_id::create("tx");
        start_item(tx);
        tx.instr = 32'h00052503; // LW x10, 0(x10)
        finish_item(tx);

        // S-type SW
        tx = decoder_transaction::type_id::create("tx");
        start_item(tx);
        tx.instr = 32'h00B52023; // SW x11, 0(x10)
        finish_item(tx);

        // B-type BEQ
        tx = decoder_transaction::type_id::create("tx");
        start_item(tx);
        tx.instr = 32'h00B50463; // BEQ x10, x11, 8
        finish_item(tx);

        // J-type JAL
        tx = decoder_transaction::type_id::create("tx");
        start_item(tx);
        tx.instr = 32'h008000EF; // JAL x1, 8
        finish_item(tx);
    endtask
endclass