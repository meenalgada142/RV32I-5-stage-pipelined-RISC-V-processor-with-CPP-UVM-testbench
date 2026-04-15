module rv32i_decoder (
    input  logic [31:0] instr,
    output logic [4:0]  rs1,
    output logic [4:0]  rs2,
    output logic [4:0]  rd,
    output logic [31:0] imm,
    output logic [3:0]  alu_op,
    output logic        reg_write,
    output logic        mem_read,
    output logic        mem_write,
    output logic        mem_to_reg,
    output logic        alu_src,
    output logic        branch,
    output logic        branch_type,  // 0=BEQ, 1=BNE
    output logic        jump
);

    // Field extractions — wire form keeps constant selects outside always @(*)
    wire [6:0] opcode = instr[6:0];
    wire [2:0] funct3 = instr[14:12];
    wire [6:0] funct7 = instr[31:25];

    assign rs1 = instr[19:15];
    assign rs2 = instr[24:20];
    assign rd  = instr[11:7];

    // Immediate pre-computations (all combinatorial, outside always block)
    wire [31:0] imm_i = {{20{instr[31]}}, instr[31:20]};
    wire [31:0] imm_s = {{20{instr[31]}}, instr[31:25], instr[11:7]};
    // B-type: imm[12]=instr[31], imm[11]=instr[7], imm[10:5]=instr[30:25], imm[4:1]=instr[11:8], imm[0]=0
    wire [12:0] imm_b = {instr[31], instr[7], instr[30:25], instr[11:8], 1'b0};
    wire [31:0] imm_j = {{11{instr[31]}}, instr[31], instr[19:12], instr[20], instr[30:21], 1'b0};

    always @(*) begin
        imm         = 32'd0;
        alu_op      = 4'd0;
        reg_write   = 1'b0;
        mem_read    = 1'b0;
        mem_write   = 1'b0;
        mem_to_reg  = 1'b0;
        alu_src     = 1'b0;
        branch      = 1'b0;
        branch_type = 1'b0;
        jump        = 1'b0;

        case (opcode)
            7'b0110011: begin // R-type (ADD, SUB, AND, OR, XOR, SLT, SLTU, SLL, SRL, SRA)
                alu_src = 1'b0;
                mem_to_reg = 1'b0;
                mem_read = 1'b0;
                mem_write = 1'b0;
                branch = 1'b0;
                jump = 1'b0;
                reg_write = 1'b1;
                imm = 32'd0;
                case (funct3)
                    3'b000: alu_op = (funct7 == 7'b0100000) ? 4'b0001 : 4'b0000; // SUB / ADD
                    3'b111: alu_op = 4'b0010; // AND
                    3'b110: alu_op = 4'b0011; // OR
                    3'b100: alu_op = 4'b0100; // XOR
                    3'b010: alu_op = 4'b0101; // SLT
                    3'b011: alu_op = 4'b1000; // SLTU
                    3'b001: alu_op = 4'b0110; // SLL
                    3'b101: alu_op = (funct7 == 7'b0100000) ? 4'b1001 : 4'b0111; // SRA / SRL
                    default: alu_op = 4'b0000;
                endcase
            end
            7'b0010011: begin // I-type (ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI)
                alu_src = 1'b1;
                mem_to_reg = 1'b0;
                mem_read = 1'b0;
                mem_write = 1'b0;
                branch = 1'b0;
                jump = 1'b0;
                reg_write = 1'b1;
                imm = imm_i;
                // Decode based on funct3
                case (funct3)
                    3'b000: alu_op = 4'b0000;  // ADDI
                    3'b010: alu_op = 4'b0101;  // SLTI (signed <)
                    3'b011: alu_op = 4'b1000;  // SLTIU (unsigned <)
                    3'b100: alu_op = 4'b0100;  // XORI
                    3'b110: alu_op = 4'b0011;  // ORI
                    3'b111: alu_op = 4'b0010;  // ANDI
                    3'b001: alu_op = 4'b0110;  // SLLI
                    3'b101: begin               // SRLI or SRAI based on funct7[5]
                        if (funct7[5])
                            alu_op = 4'b1001;  // SRAI
                        else
                            alu_op = 4'b0111;  // SRLI
                    end
                    default: alu_op = 4'b0000;
                endcase
            end
            7'b0000011: begin // LW
                alu_src = 1'b1;
                mem_to_reg = 1'b1;
                mem_read = 1'b1;
                mem_write = 1'b0;
                branch = 1'b0;
                jump = 1'b0;
                reg_write = 1'b1;
                imm = imm_i;
                alu_op = 4'b0000;
            end
            7'b0100011: begin // SW
                alu_src = 1'b1;
                mem_to_reg = 1'b0;
                mem_read = 1'b0;
                mem_write = 1'b1;
                branch = 1'b0;
                jump = 1'b0;
                reg_write = 1'b0;
                imm = imm_s;
                alu_op = 4'b0000;
            end
            7'b1100011: begin // BEQ/BNE
                alu_src = 1'b0;
                mem_to_reg = 1'b0;
                mem_read = 1'b0;
                mem_write = 1'b0;
                branch = 1'b1;
                branch_type = funct3[0];  // 0=BEQ (funct3=000), 1=BNE (funct3=001)
                jump = 1'b0;
                reg_write = 1'b0;
                imm = {{19{imm_b[12]}}, imm_b};
                case (funct3)
                    3'b000: alu_op = 4'b0001; // BEQ
                    3'b001: alu_op = 4'b0001; // BNE
                    default: alu_op = 4'b0001;
                endcase
            end
            7'b1101111: begin // JAL
                alu_src = 1'b1;  // Use PC+4 (imm_j holds offset)
                mem_to_reg = 1'b0;
                mem_read = 1'b0;
                mem_write = 1'b0;
                branch = 1'b0;
                jump = 1'b1;
                reg_write = 1'b1;
                imm = imm_j;
                alu_op = 4'b0000;
            end
            default: begin
                imm         = 32'd0;
                alu_op      = 4'b0000;
                reg_write   = 1'b0;
                mem_read    = 1'b0;
                mem_write   = 1'b0;
                mem_to_reg  = 1'b0;
                alu_src     = 1'b0;
                branch      = 1'b0;
                branch_type = 1'b0;
                jump        = 1'b0;
            end
        endcase
    end
endmodule
