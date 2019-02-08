// Author: Erwin Ouyang
// Date  : 6 Jan 2018
// Update: 15 Jul 2018 - Porting to RedPitaya
//	       04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa

`timescale 1ns / 1ps

module axis_bpsk_mod
    (
        // *** AXI4 clock and reset port ***
        input wire         aclk,
        input wire         aresetn,
        // *** AXI4-stream slave port ***
        output wire        s_axis_tready,
        input wire [31:0]  s_axis_tdata,
        input wire         s_axis_tvalid,
        input wire         s_axis_tlast,
        // *** AXI4-stream master port ***
        input wire         m_axis_tready,
        output wire [31:0] m_axis_tdata,
        output wire        m_axis_tvalid,
        output wire        m_axis_tlast,
        // *** User port ***
        input wire         en
    );
    
    localparam S_READ = 2'h0,
               S_MAP = 2'h1,
               S_WRITE = 2'h2;
    
    reg [1:0] _cs, _ns;
    reg [4:0] cnt_map_cv, cnt_map_nv;
    reg [5:0] cnt_wr_cv, cnt_wr_nv;
    wire data_in_w;
    wire [31:0] data_mod_w, data_conj_w;
    reg m_axis_tlast_cv, m_axis_tlast_nv;
    reg data_mem [0:31];
    reg [31:0] subcar_mem [0:63];
    integer i;
    
    // *** BPSK modulator ***
    bpsk_mod bpsk_mod_0
    (
        .data_in(data_in_w),
        .data_mod(data_mod_w),
        .data_conj(data_conj_w)
    );

    assign data_in_w = data_mem[cnt_map_cv];
    assign s_axis_tready = (_cs == S_READ) ? 1 : 0;
    assign m_axis_tdata = subcar_mem[cnt_wr_cv];
    assign m_axis_tvalid = ((_cs == S_WRITE)) ? 1 : 0;
    assign m_axis_tlast = m_axis_tlast_cv;
        
    // *** Data memory for storing grouped bit ***
    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            for (i = 0; i <= 31; i = i+1)
                data_mem[i] <= 0;
        end
        else
        begin
            if (en)
            begin
                if (s_axis_tvalid)
                begin
                    for (i = 0; i <= 31; i = i+1)
                        data_mem[i] <= s_axis_tdata[31-i];
                end
            end
        end
    end

    // *** Subcarrier memory for storing subcarrier data ***
    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            for (i = 0; i <= 63; i = i+1)
                subcar_mem[i] <= 0;
        end
        else
        begin
            if (en)
            begin
                if (_cs == S_MAP)
                begin
                    subcar_mem[cnt_map_cv+1] <= data_mod_w;
                    subcar_mem[64-cnt_map_cv-1] <= data_conj_w;
                end
            end
        end
    end
    
    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            _cs <= S_READ;
            cnt_map_cv <= 0;
            cnt_wr_cv <= 0;
            m_axis_tlast_cv <= 0;
        end
        else
        begin
            if (en)
            begin
                _cs <= _ns;
                cnt_map_cv <= cnt_map_nv;
                cnt_wr_cv <= cnt_wr_nv;
                m_axis_tlast_cv <= m_axis_tlast_nv;
            end
        end
    end

    always @(*)
    begin
        _ns = _cs;
        cnt_map_nv = cnt_map_cv;
        cnt_wr_nv = cnt_wr_cv;
        m_axis_tlast_nv = m_axis_tlast_cv;
        if (en)
        begin
            case (_cs)
                S_READ:
                begin
                    if (s_axis_tvalid)
                    begin
                        _ns = S_MAP; 
                    end
                end
                S_MAP:
                begin
                    cnt_map_nv = cnt_map_cv + 1;
                    if (cnt_map_cv == 30)
                    begin
                        _ns = S_WRITE; 
                        cnt_map_nv = 0;
                    end
                end
                S_WRITE:
                begin
                    if (m_axis_tready)
                    begin
                        if (cnt_wr_cv == 63)
                        begin
                            _ns = S_READ;
                            cnt_wr_nv = 0;
                            m_axis_tlast_nv = 0;
                        end
                        else
                        begin
                            cnt_wr_nv = cnt_wr_cv + 1;
                            if (cnt_wr_cv == 62)
                                m_axis_tlast_nv = 1;
                        end
                    end
                end
            endcase
        end
    end
          
endmodule
