// Author: Erwin Ouyang
// Date  : 15 Jan 2018
// Update: 01 Jul 2018 - Porting to RedPitaya
//	       04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa


`timescale 1ns / 1ps

module axis_qam16_demod
    (
        // *** AXI4 clock and reset port ***
        input wire         aclk,
        input wire         aresetn,
        // *** AXI4-stream slave port ***
        output wire        s_axis_tready,
        input wire [47:0]  s_axis_tdata,
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
               S_WRITE = 2'h1;
                 
    reg [1:0] _cs, _ns;
    reg [5:0] cnt_rd_cv, cnt_rd_nv;
    reg [4:0] cnt_wr_cv, cnt_wr_nv;
    wire [22:0] data_in_re_w, data_in_im_w;
    wire [3:0] data_demod_w;    
    reg m_axis_tlast_cv, m_axis_tlast_nv;
    reg [3:0] data_demod [0:31];
    integer i;

    qam16_demod qam16_demod_0
    (
        .data_in_re(data_in_re_w),
        .data_in_im(data_in_im_w),
        .data_demod(data_demod_w)
    );
    
    assign data_in_re_w = s_axis_tdata[22:0];
    assign data_in_im_w = s_axis_tdata[46:24];
    assign s_axis_tready = (_cs == S_READ) ? 1 : 0;
    assign m_axis_tdata = (cnt_wr_cv <= 17) ? 
                          {data_demod[cnt_wr_cv+0], data_demod[cnt_wr_cv+1],
                           data_demod[cnt_wr_cv+2], data_demod[cnt_wr_cv+3],
                           data_demod[cnt_wr_cv+4], data_demod[cnt_wr_cv+5],
                           data_demod[cnt_wr_cv+6], data_demod[cnt_wr_cv+7]} :
                          {data_demod[cnt_wr_cv+0], data_demod[cnt_wr_cv+1],
                           data_demod[cnt_wr_cv+2], data_demod[cnt_wr_cv+3],
                           data_demod[cnt_wr_cv+4], data_demod[cnt_wr_cv+5],
                           data_demod[cnt_wr_cv+6], 4'b0000};
    assign m_axis_tvalid = ((_cs == S_WRITE)) ? 1 : 0;
    assign m_axis_tlast = m_axis_tlast_cv;
        
    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            for (i = 0; i <= 31; i = i+1)
                data_demod[i] <= 0;
        end
        else
        begin
            if (en)
            begin
                if (s_axis_tvalid)
                    data_demod[cnt_rd_cv] <= data_demod_w;
            end
        end
    end

    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            _cs <= S_READ;
            cnt_rd_cv <= 0;
            cnt_wr_cv <= 1;
            m_axis_tlast_cv <= 0;
        end
        else
        begin
            if (en)
            begin
                _cs <= _ns;
                cnt_rd_cv <= cnt_rd_nv;
                cnt_wr_cv <= cnt_wr_nv;
                m_axis_tlast_cv <= m_axis_tlast_nv;
            end
        end
    end
    
    always @(*)
    begin
        _ns = _cs;
        cnt_rd_nv = cnt_rd_cv;
        cnt_wr_nv = cnt_wr_cv;
        m_axis_tlast_nv = m_axis_tlast_cv;
        if (en)
        begin
            case (_cs)
                S_READ:
                begin
                    if (s_axis_tvalid)
                    begin
                        cnt_rd_nv = cnt_rd_cv + 1;
                        if (cnt_rd_cv == 63)
                        begin
                            _ns = S_WRITE; 
                            cnt_rd_nv = 0;
                        end
                    end
                end
                S_WRITE:
                begin
                    if (m_axis_tready)
                    begin
                        if (cnt_wr_cv == 25)
                        begin
                            _ns = S_READ;
                            cnt_wr_nv = 1;
                            m_axis_tlast_nv = 0;
                        end
                        else
                        begin
                            cnt_wr_nv = cnt_wr_cv + 8;
                            if (cnt_wr_cv == 17)
                                m_axis_tlast_nv = 1;
                        end
                    end
                end
            endcase
        end
    end
    
endmodule
