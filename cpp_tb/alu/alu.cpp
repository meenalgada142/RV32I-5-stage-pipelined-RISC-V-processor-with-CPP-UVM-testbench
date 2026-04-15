#include "alu.h"

ALU::ALU() : a_(0), b_(0), alu_op_(0), result_(0), zero_(false) {}

ALU::~ALU() {}

void ALU::set_inputs(uint32_t a, uint32_t b, uint8_t alu_op) {
    a_ = a;
    b_ = b;
    alu_op_ = alu_op;
    compute();
}

uint32_t ALU::get_result() const {
    return result_;
}

bool ALU::get_zero() const {
    return zero_;
}

void ALU::compute() {
    switch (alu_op_) {
        case 0: // ADD
            result_ = a_ + b_;
            break;
        case 1: // SUB
            result_ = a_ - b_;
            break;
        case 2: // AND
            result_ = a_ & b_;
            break;
        case 3: // OR
            result_ = a_ | b_;
            break;
        case 4: // XOR
            result_ = a_ ^ b_;
            break;
        case 5: // SLT
            result_ = (static_cast<int32_t>(a_) < static_cast<int32_t>(b_)) ? 1 : 0;
            break;
        case 6: // SLL
            result_ = a_ << (b_ & 0x1F);
            break;
        case 7: // SRL
            result_ = a_ >> (b_ & 0x1F);
            break;
        case 8: // SLTU
            result_ = (a_ < b_) ? 1 : 0;
            break;
        case 9: // SRA
            result_ = static_cast<uint32_t>(static_cast<int32_t>(a_) >> (b_ & 0x1F));
            break;
        default:
            result_ = 0;
            break;
    }
    zero_ = (result_ == 0);
}