// Author: Erwin Ouyang
// Date  : 6 Jan 2018
// Update: 15 Jul 2018 - Porting to RedPitaya
//	       04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa

`timescale 1ns / 1ps

module axis_modulator
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
        input wire [1:0]   mod_type
    );
    
    wire s_axis_tready_bpsk, s_axis_tready_qpsk, s_axis_tready_qam16;
    wire [31:0] m_axis_tdata_bpsk, m_axis_tdata_qpsk, m_axis_tdata_qam16;
    wire m_axis_tvalid_bpsk, m_axis_tvalid_qpsk, m_axis_tvalid_qam16;
    wire m_axis_tlast_bpsk, m_axis_tlast_qpsk, m_axis_tlast_qam16;
    wire en_bpsk, en_qpsk, en_qam16;
    
    assign s_axis_tready = (mod_type == 0) ? s_axis_tready_bpsk : 
                           (mod_type == 1) ? s_axis_tready_qpsk :
                           (mod_type == 2) ? s_axis_tready_qam16 : 0;
    assign m_axis_tdata = (mod_type == 0) ? m_axis_tdata_bpsk : 
                          (mod_type == 1) ? m_axis_tdata_qpsk :
                          (mod_type == 2) ? m_axis_tdata_qam16 : 0;
    assign m_axis_tvalid = (mod_type == 0) ? m_axis_tvalid_bpsk : 
                           (mod_type == 1) ? m_axis_tvalid_qpsk :
                           (mod_type == 2) ? m_axis_tvalid_qam16 : 0;
    assign m_axis_tlast = (mod_type == 0) ? m_axis_tlast_bpsk : 
                          (mod_type == 1) ? m_axis_tlast_qpsk :
                          (mod_type == 2) ? m_axis_tlast_qam16 : 0;
   
    assign en_bpsk = (mod_type == 0) ? 1 : 0;
    assign en_qpsk = (mod_type == 1) ? 1 : 0;
    assign en_qam16 = (mod_type == 2) ? 1 : 0;

    axis_bpsk_mod axis_bpsk_mod_0
    (
        .aclk(aclk),
        .aresetn(aresetn),
        .s_axis_tready(s_axis_tready_bpsk),
        .s_axis_tdata(s_axis_tdata),
        .s_axis_tvalid(s_axis_tvalid),
        .s_axis_tlast(s_axis_tlast),       
        .m_axis_tready(m_axis_tready),
        .m_axis_tdata(m_axis_tdata_bpsk),
        .m_axis_tvalid(m_axis_tvalid_bpsk),
        .m_axis_tlast(m_axis_tlast_bpsk),
        .en(en_bpsk) 
    );
   
    axis_qpsk_mod axis_qpsk_mod_0
    (
        .aclk(aclk),
        .aresetn(aresetn),
        .s_axis_tready(s_axis_tready_qpsk),
        .s_axis_tdata(s_axis_tdata),
        .s_axis_tvalid(s_axis_tvalid),
        .s_axis_tlast(s_axis_tlast),       
        .m_axis_tready(m_axis_tready),
        .m_axis_tdata(m_axis_tdata_qpsk),
        .m_axis_tvalid(m_axis_tvalid_qpsk),
        .m_axis_tlast(m_axis_tlast_qpsk),
        .en(en_qpsk) 
    );
                                   
    axis_qam16_mod axis_qam16_mod_0
    (
        .aclk(aclk),
        .aresetn(aresetn),
        .s_axis_tready(s_axis_tready_qam16),
        .s_axis_tdata(s_axis_tdata),
        .s_axis_tvalid(s_axis_tvalid),
        .s_axis_tlast(s_axis_tlast),       
        .m_axis_tready(m_axis_tready),
        .m_axis_tdata(m_axis_tdata_qam16),
        .m_axis_tvalid(m_axis_tvalid_qam16),
        .m_axis_tlast(m_axis_tlast_qam16),
        .en(en_qam16)  
    );
    
endmodule
