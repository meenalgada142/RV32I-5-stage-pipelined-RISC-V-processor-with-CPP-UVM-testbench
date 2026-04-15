// Decoder Agent
class decoder_agent extends uvm_agent;
    `uvm_component_utils(decoder_agent)

    decoder_driver drv;
    decoder_monitor mon;
    uvm_sequencer#(decoder_transaction) seqr;

    function new(string name, uvm_component parent);
        super.new(name, parent);
    endfunction

    function void build_phase(uvm_phase phase);
        super.build_phase(phase);
        mon = decoder_monitor::type_id::create("mon", this);
        if (get_is_active() == UVM_ACTIVE) begin
            drv = decoder_driver::type_id::create("drv", this);
            seqr = uvm_sequencer#(decoder_transaction)::type_id::create("seqr", this);
        end
    endfunction

    function void connect_phase(uvm_phase phase);
        if (get_is_active() == UVM_ACTIVE) begin
            drv.seq_item_port.connect(seqr.seq_item_export);
        end
    endfunction
endclass