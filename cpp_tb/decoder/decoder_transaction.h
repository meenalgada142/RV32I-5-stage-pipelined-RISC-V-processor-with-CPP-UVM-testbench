#pragma once
// decoder_transaction.h — stimulus + response bundle (uvm_sequence_item equivalent)
//
// A DecoderTransaction carries:
//   - The raw 32-bit instruction (stimulus)
//   - The expected decoded fields  (from golden model, filled by driver)
//   - The actual decoded fields    (from DUT, filled by driver)
//   - A human-readable label       (for error messages)

#include <cstdint>
#include <string>

struct DecoderTransaction {
    // ---- Stimulus ----
    uint32_t    instr = 0;
    std::string label;

    // ---- Expected (golden) ----
    uint8_t  exp_rs1        = 0;
    uint8_t  exp_rs2        = 0;
    uint8_t  exp_rd         = 0;
    uint32_t exp_imm        = 0;
    uint8_t  exp_alu_op     = 0;
    bool     exp_reg_write  = false;
    bool     exp_mem_read   = false;
    bool     exp_mem_write  = false;
    bool     exp_mem_to_reg = false;
    bool     exp_alu_src    = false;
    bool     exp_branch     = false;
    bool     exp_branch_type= false;
    bool     exp_jump       = false;

    // ---- Actual (DUT) ----
    uint8_t  act_rs1        = 0;
    uint8_t  act_rs2        = 0;
    uint8_t  act_rd         = 0;
    uint32_t act_imm        = 0;
    uint8_t  act_alu_op     = 0;
    bool     act_reg_write  = false;
    bool     act_mem_read   = false;
    bool     act_mem_write  = false;
    bool     act_mem_to_reg = false;
    bool     act_alu_src    = false;
    bool     act_branch     = false;
    bool     act_branch_type= false;
    bool     act_jump       = false;

    DecoderTransaction() = default;
    explicit DecoderTransaction(uint32_t instr_, std::string label_ = "")
        : instr(instr_), label(std::move(label_)) {}
};
