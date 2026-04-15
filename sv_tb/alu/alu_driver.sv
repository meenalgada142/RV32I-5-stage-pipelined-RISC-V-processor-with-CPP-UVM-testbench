// ALU Driver
class alu_driver extends uvm_driver#(alu_transaction);
    `uvm_component_utils(alu_driver)

    virtual alu_if.DRV vif;

    function new(string name, uvm_component parent);
        super.new(name, parent);
    endfunction

    function void build_phase(uvm_phase phase);
        super.build_phase(phase);
        if (!uvm_config_db#(virtual alu_if.DRV)::get(this, "", "vif", vif))
            `uvm_fatal("DRV", "Virtual interface not found")
    endfunction

    task run_phase(uvm_phase phase);
        alu_transaction tx;
        forever begin
            seq_item_port.get_next_item(tx);
            `uvm_info("DRV", $sformatf("Driving transaction: a=%h, b=%h, op=%d", tx.a, tx.b, tx.alu_op), UVM_MEDIUM)
            vif.drv_cb.a <= tx.a;
            vif.drv_cb.b <= tx.b;
            vif.drv_cb.alu_op <= tx.alu_op;
            @(vif.drv_cb);
            seq_item_port.item_done();
        end
    endtask
endclass