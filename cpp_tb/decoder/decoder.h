#ifndef DECODER_H
#define DECODER_H

#include <cstdint>

struct DecodedInstruction {
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
    uint32_t imm;
    uint8_t alu_op;
    bool reg_write;
    bool mem_read;
    bool mem_write;
    bool mem_to_reg;
    bool alu_src;
    bool branch;
    bool branch_type;
    bool jump;
};

class Decoder {
public:
    Decoder();
    ~Decoder();

    DecodedInstruction decode(uint32_t instr);

private:
    // Helper functions
    uint32_t sign_extend(uint32_t value, int bits);
};

#endif // DECODER_H