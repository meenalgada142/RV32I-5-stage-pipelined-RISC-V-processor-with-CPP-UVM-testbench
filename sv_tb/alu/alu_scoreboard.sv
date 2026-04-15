// ALU Scoreboard
class alu_scoreboard extends uvm_scoreboard;
    `uvm_component_utils(alu_scoreboard)

    uvm_analysis_imp#(alu_transaction, alu_scoreboard) imp;

    function new(string name, uvm_component parent);
        super.new(name, parent);
        imp = new("imp", this);
    endfunction

    function void write(alu_transaction tx);
        logic [31:0] expected_result;
        logic expected_zero;

        // Reference model
        case (tx.alu_op)
            4'b0000: expected_result = tx.a + tx.b; // ADD
            4'b0001: expected_result = tx.a - tx.b; // SUB
            4'b0010: expected_result = tx.a & tx.b; // AND
            4'b0011: expected_result = tx.a | tx.b; // OR
            4'b0100: expected_result = tx.a ^ tx.b; // XOR
            4'b0101: expected_result = {31'b0, ($signed(tx.a) < $signed(tx.b))}; // SLT
            4'b0110: expected_result = tx.a << tx.b[4:0]; // SLL
            4'b0111: expected_result = tx.a >> tx.b[4:0]; // SRL
            4'b1000: expected_result = {31'b0, (tx.a < tx.b)}; // SLTU
            4'b1001: expected_result = $signed(tx.a) >>> tx.b[4:0]; // SRA
            default: expected_result = 32'h00000000;
        endcase
        expected_zero = (expected_result == 32'h00000000);

        if (tx.result !== expected_result || tx.zero !== expected_zero) begin
            `uvm_error("SCB", $sformatf("Mismatch! Expected result=%h zero=%b, Got result=%h zero=%b",
                        expected_result, expected_zero, tx.result, tx.zero))
        end else begin
            `uvm_info("SCB", "Match!", UVM_MEDIUM)
        end
    endfunction
endclass