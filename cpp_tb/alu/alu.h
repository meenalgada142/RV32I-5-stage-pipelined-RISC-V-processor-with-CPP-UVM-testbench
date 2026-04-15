#ifndef ALU_H
#define ALU_H

#include <cstdint>

class ALU {
public:
    ALU();
    ~ALU();

    void set_inputs(uint32_t a, uint32_t b, uint8_t alu_op);
    uint32_t get_result() const;
    bool get_zero() const;

private:
    uint32_t a_;
    uint32_t b_;
    uint8_t alu_op_;
    uint32_t result_;
    bool zero_;

    void compute();
};

#endif // ALU_H