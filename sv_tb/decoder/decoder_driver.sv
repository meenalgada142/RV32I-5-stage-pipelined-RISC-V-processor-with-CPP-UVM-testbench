// Decoder Driver
class decoder_driver extends uvm_driver#(decoder_transaction);
    `uvm_component_utils(decoder_driver)

    virtual decoder_if.DRV vif;

    function new(string name, uvm_component parent);
        super.new(name, parent);
    endfunction

    function void build_phase(uvm_phase phase);
        super.build_phase(phase);
        if (!uvm_config_db#(virtual decoder_if.DRV)::get(this, "", "vif", vif))
            `uvm_fatal("DRV", "Virtual interface not found")
    endfunction

    task run_phase(uvm_phase phase);
        decoder_transaction tx;
        forever begin
            seq_item_port.get_next_item(tx);
            `uvm_info("DRV", $sformatf("Driving instruction: %h", tx.instr), UVM_MEDIUM)
            vif.drv_cb.instr <= tx.instr;
            @(vif.drv_cb);
            seq_item_port.item_done();
        end
    endtask
endclass