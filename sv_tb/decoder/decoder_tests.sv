// Decoder Tests
class decoder_base_test extends uvm_test;
    `uvm_component_utils(decoder_base_test)

    decoder_env env;

    function new(string name, uvm_component parent);
        super.new(name, parent);
    endfunction

    function void build_phase(uvm_phase phase);
        super.build_phase(phase);
        env = decoder_env::type_id::create("env", this);
    endfunction

    task run_phase(uvm_phase phase);
        decoder_base_sequence seq;
        phase.raise_objection(this);
        seq = decoder_base_sequence::type_id::create("seq");
        seq.start(env.agt.seqr);
        phase.drop_objection(this);
    endtask
endclass

class decoder_random_test extends decoder_base_test;
    `uvm_component_utils(decoder_random_test)

    function new(string name, uvm_component parent);
        super.new(name, parent);
    endfunction

    task run_phase(uvm_phase phase);
        decoder_random_sequence seq;
        phase.raise_objection(this);
        seq = decoder_random_sequence::type_id::create("seq");
        seq.start(env.agt.seqr);
        phase.drop_objection(this);
    endtask
endclass

class decoder_stress_test extends decoder_base_test;
    `uvm_component_utils(decoder_stress_test)

    function new(string name, uvm_component parent);
        super.new(name, parent);
    endfunction

    task run_phase(uvm_phase phase);
        decoder_corner_sequence seq;
        phase.raise_objection(this);
        seq = decoder_corner_sequence::type_id::create("seq");
        seq.start(env.agt.seqr);
        phase.drop_objection(this);
    endtask
endclass