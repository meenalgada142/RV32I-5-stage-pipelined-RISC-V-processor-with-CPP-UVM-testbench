// ALU Tests
class alu_base_test extends uvm_test;
    `uvm_component_utils(alu_base_test)

    alu_env env;

    function new(string name, uvm_component parent);
        super.new(name, parent);
    endfunction

    function void build_phase(uvm_phase phase);
        super.build_phase(phase);
        env = alu_env::type_id::create("env", this);
    endfunction

    task run_phase(uvm_phase phase);
        alu_base_sequence seq;
        phase.raise_objection(this);
        seq = alu_base_sequence::type_id::create("seq");
        seq.start(env.agt.seqr);
        phase.drop_objection(this);
    endtask
endclass

class alu_random_test extends alu_base_test;
    `uvm_component_utils(alu_random_test)

    function new(string name, uvm_component parent);
        super.new(name, parent);
    endfunction

    task run_phase(uvm_phase phase);
        alu_random_sequence seq;
        phase.raise_objection(this);
        seq = alu_random_sequence::type_id::create("seq");
        seq.start(env.agt.seqr);
        phase.drop_objection(this);
    endtask
endclass

class alu_stress_test extends alu_base_test;
    `uvm_component_utils(alu_stress_test)

    function new(string name, uvm_component parent);
        super.new(name, parent);
    endfunction

    task run_phase(uvm_phase phase);
        alu_corner_sequence seq;
        phase.raise_objection(this);
        seq = alu_corner_sequence::type_id::create("seq");
        seq.start(env.agt.seqr);
        phase.drop_objection(this);
    endtask
endclass