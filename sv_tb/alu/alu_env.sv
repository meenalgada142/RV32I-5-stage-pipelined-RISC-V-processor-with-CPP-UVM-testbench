// ALU Environment
class alu_env extends uvm_env;
    `uvm_component_utils(alu_env)

    alu_agent agt;
    alu_scoreboard scb;
    alu_coverage cov;

    function new(string name, uvm_component parent);
        super.new(name, parent);
    endfunction

    function void build_phase(uvm_phase phase);
        super.build_phase(phase);
        agt = alu_agent::type_id::create("agt", this);
        scb = alu_scoreboard::type_id::create("scb", this);
        cov = alu_coverage::type_id::create("cov", this);
    endfunction

    function void connect_phase(uvm_phase phase);
        agt.mon.ap.connect(scb.imp);
        agt.mon.ap.connect(cov.analysis_export);
    endfunction
endclass