// Decoder Monitor
class decoder_monitor extends uvm_monitor;
    `uvm_component_utils(decoder_monitor)

    virtual decoder_if.MON vif;
    uvm_analysis_port#(decoder_transaction) ap;

    function new(string name, uvm_component parent);
        super.new(name, parent);
        ap = new("ap", this);
    endfunction

    function void build_phase(uvm_phase phase);
        super.build_phase(phase);
        if (!uvm_config_db#(virtual decoder_if.MON)::get(this, "", "vif", vif))
            `uvm_fatal("MON", "Virtual interface not found")
    endfunction

    task run_phase(uvm_phase phase);
        decoder_transaction tx;
        forever begin
            @(vif.mon_cb);
            tx = decoder_transaction::type_id::create("tx");
            tx.instr = vif.mon_cb.instr;
            tx.rs1 = vif.mon_cb.rs1;
            tx.rs2 = vif.mon_cb.rs2;
            tx.rd = vif.mon_cb.rd;
            tx.imm = vif.mon_cb.imm;
            tx.alu_op = vif.mon_cb.alu_op;
            tx.reg_write = vif.mon_cb.reg_write;
            tx.mem_read = vif.mon_cb.mem_read;
            tx.mem_write = vif.mon_cb.mem_write;
            tx.mem_to_reg = vif.mon_cb.mem_to_reg;
            tx.alu_src = vif.mon_cb.alu_src;
            tx.branch = vif.mon_cb.branch;
            tx.branch_type = vif.mon_cb.branch_type;
            tx.jump = vif.mon_cb.jump;
            `uvm_info("MON", $sformatf("Monitored: opcode=%b", tx.instr[6:0]), UVM_MEDIUM)
            ap.write(tx);
        end
    endtask
endclass