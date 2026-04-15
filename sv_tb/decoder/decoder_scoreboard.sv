// Decoder Scoreboard
class decoder_scoreboard extends uvm_scoreboard;
    `uvm_component_utils(decoder_scoreboard)

    uvm_analysis_imp#(decoder_transaction, decoder_scoreboard) imp;

    function new(string name, uvm_component parent);
        super.new(name, parent);
        imp = new("imp", this);
    endfunction

    function void write(decoder_transaction tx);
        decoder_transaction expected = new("expected");

        // Reference model: decode the instruction
        expected.instr = tx.instr;
        expected.rs1 = tx.instr[19:15];
        expected.rs2 = tx.instr[24:20];
        expected.rd = tx.instr[11:7];

        logic [6:0] opcode = tx.instr[6:0];
        logic [2:0] funct3 = tx.instr[14:12];
        logic [6:0] funct7 = tx.instr[31:25];

        // Default values
        expected.imm = 32'd0;
        expected.alu_op = 4'd0;
        expected.reg_write = 1'b0;
        expected.mem_read = 1'b0;
        expected.mem_write = 1'b0;
        expected.mem_to_reg = 1'b0;
        expected.alu_src = 1'b0;
        expected.branch = 1'b0;
        expected.branch_type = 1'b0;
        expected.jump = 1'b0;

        case (opcode)
            7'b0110011: begin // R-type
                expected.alu_src = 1'b0;
                expected.mem_to_reg = 1'b0;
                expected.mem_read = 1'b0;
                expected.mem_write = 1'b0;
                expected.branch = 1'b0;
                expected.jump = 1'b0;
                expected.reg_write = 1'b1;
                case (funct3)
                    3'b000: expected.alu_op = (funct7 == 7'b0100000) ? 4'b0001 : 4'b0000;
                    3'b111: expected.alu_op = 4'b0010;
                    3'b110: expected.alu_op = 4'b0011;
                    3'b100: expected.alu_op = 4'b0100;
                    3'b010: expected.alu_op = 4'b0101;
                    3'b011: expected.alu_op = 4'b1000;
                    3'b001: expected.alu_op = 4'b0110;
                    3'b101: expected.alu_op = (funct7 == 7'b0100000) ? 4'b1001 : 4'b0111;
                endcase
            end
            7'b0010011: begin // I-type
                expected.alu_src = 1'b1;
                expected.mem_to_reg = 1'b0;
                expected.mem_read = 1'b0;
                expected.mem_write = 1'b0;
                expected.branch = 1'b0;
                expected.jump = 1'b0;
                expected.reg_write = 1'b1;
                expected.imm = {{20{tx.instr[31]}}, tx.instr[31:20]};
                case (funct3)
                    3'b000: expected.alu_op = 4'b0000;
                    3'b010: expected.alu_op = 4'b0101;
                    3'b011: expected.alu_op = 4'b1000;
                    3'b100: expected.alu_op = 4'b0100;
                    3'b110: expected.alu_op = 4'b0011;
                    3'b111: expected.alu_op = 4'b0010;
                    3'b001: expected.alu_op = 4'b0110;
                    3'b101: expected.alu_op = (funct7[5]) ? 4'b1001 : 4'b0111;
                endcase
            end
            7'b0000011: begin // LW
                expected.alu_src = 1'b1;
                expected.mem_to_reg = 1'b1;
                expected.mem_read = 1'b1;
                expected.mem_write = 1'b0;
                expected.branch = 1'b0;
                expected.jump = 1'b0;
                expected.reg_write = 1'b1;
                expected.imm = {{20{tx.instr[31]}}, tx.instr[31:20]};
                expected.alu_op = 4'b0000;
            end
            7'b0100011: begin // SW
                expected.alu_src = 1'b1;
                expected.mem_to_reg = 1'b0;
                expected.mem_read = 1'b0;
                expected.mem_write = 1'b1;
                expected.branch = 1'b0;
                expected.jump = 1'b0;
                expected.reg_write = 1'b0;
                expected.imm = {{20{tx.instr[31]}}, tx.instr[31:25], tx.instr[11:7]};
                expected.alu_op = 4'b0000;
            end
            7'b1100011: begin // BEQ/BNE
                expected.alu_src = 1'b0;
                expected.mem_to_reg = 1'b0;
                expected.mem_read = 1'b0;
                expected.mem_write = 1'b0;
                expected.branch = 1'b1;
                expected.branch_type = funct3[0];
                expected.jump = 1'b0;
                expected.reg_write = 1'b0;
                logic [12:0] imm_b = {tx.instr[31], tx.instr[7], tx.instr[30:25], tx.instr[11:8], 1'b0};
                expected.imm = {{19{imm_b[12]}}, imm_b};
                expected.alu_op = 4'b0001;
            end
            7'b1101111: begin // JAL
                expected.alu_src = 1'b1;
                expected.mem_to_reg = 1'b0;
                expected.mem_read = 1'b0;
                expected.mem_write = 1'b0;
                expected.branch = 1'b0;
                expected.jump = 1'b1;
                expected.reg_write = 1'b1;
                logic [20:0] imm_j = {tx.instr[31], tx.instr[19:12], tx.instr[20], tx.instr[30:21], 1'b0};
                expected.imm = {{11{imm_j[20]}}, imm_j};
                expected.alu_op = 4'b0000;
            end
        endcase

        if (!tx.do_compare(expected)) begin
            `uvm_error("SCB", "Decoding mismatch!")
            tx.print();
            expected.print();
        end else begin
            `uvm_info("SCB", "Decoding match!", UVM_MEDIUM)
        end
    endfunction
endclass