#pragma once
// alu_if.h — Virtual interface abstraction (UVM virtual interface equivalent)
//
// In SystemVerilog UVM:
//   interface alu_if;
//     logic [31:0] a, b, result;
//     logic [3:0]  alu_op;
//     logic        zero;
//   endinterface
//   class alu_driver extends uvm_driver#(alu_tx);
//     virtual alu_if vif;   // <-- bound at runtime via config_db
//   endclass
//
// Here, ALUInterface is the pure-abstract "virtual interface" handle.
// Two concrete adapters bind it to either the C++ behavioral model or
// the Verilator-generated class — chosen at compile time / construction time
// without touching driver or monitor code.

#include <cstdint>
#include "alu.h"

// ---------------------------------------------------------------------------
// Abstract port bundle — the only thing drivers/monitors touch
// ---------------------------------------------------------------------------
class ALUInterface {
public:
    virtual ~ALUInterface() = default;

    // Drive side: apply stimulus and clock the DUT one cycle.
    virtual void     drive(uint32_t a, uint32_t b, uint8_t alu_op) = 0;

    // Monitor side: read outputs after evaluation.
    virtual uint32_t result() const = 0;
    virtual bool     zero()   const = 0;
};

// ---------------------------------------------------------------------------
// Adapter A: C++ behavioral model  (used by unit tests)
// ---------------------------------------------------------------------------
class ALUModelInterface : public ALUInterface {
public:
    explicit ALUModelInterface(ALU& dut) : dut_(dut) {}

    void     drive(uint32_t a, uint32_t b, uint8_t op) override {
        dut_.set_inputs(a, b, op);
    }
    uint32_t result() const override { return dut_.get_result(); }
    bool     zero()   const override { return dut_.get_zero(); }

private:
    ALU& dut_;
};

// ---------------------------------------------------------------------------
// Adapter B: Verilator RTL model  (compiled when Vrv32i_alu.h is present)
//
// Usage:
//   #include "Vrv32i_alu.h"
//   #include "alu_if.h"
//   Vrv32i_alu rtl;
//   ALUVerilatorInterface vif(rtl);
// ---------------------------------------------------------------------------
#ifdef VERILATED_VRV32I_ALU_H_
class ALUVerilatorInterface : public ALUInterface {
public:
    explicit ALUVerilatorInterface(Vrv32i_alu& dut) : dut_(dut) {}

    void drive(uint32_t a, uint32_t b, uint8_t op) override {
        dut_.a      = a;
        dut_.b      = b;
        dut_.alu_op = op;
        dut_.eval();
    }
    uint32_t result() const override { return static_cast<uint32_t>(dut_.result); }
    bool     zero()   const override { return static_cast<bool>(dut_.zero); }

private:
    Vrv32i_alu& dut_;
};
#endif
