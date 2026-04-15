// ALU Monitor
class alu_monitor extends uvm_monitor;
    `uvm_component_utils(alu_monitor)

    virtual alu_if.MON vif;
    uvm_analysis_port#(alu_transaction) ap;

    function new(string name, uvm_component parent);
        super.new(name, parent);
        ap = new("ap", this);
    endfunction

    function void build_phase(uvm_phase phase);
        super.build_phase(phase);
        if (!uvm_config_db#(virtual alu_if.MON)::get(this, "", "vif", vif))
            `uvm_fatal("MON", "Virtual interface not found")
    endfunction

    task run_phase(uvm_phase phase);
        alu_transaction tx;
        forever begin
            @(vif.mon_cb);
            tx = alu_transaction::type_id::create("tx");
            tx.a = vif.mon_cb.a;
            tx.b = vif.mon_cb.b;
            tx.alu_op = vif.mon_cb.alu_op;
            tx.result = vif.mon_cb.result;
            tx.zero = vif.mon_cb.zero;
            `uvm_info("MON", $sformatf("Monitored: result=%h, zero=%b", tx.result, tx.zero), UVM_MEDIUM)
            ap.write(tx);
        end
    endtask
endclass