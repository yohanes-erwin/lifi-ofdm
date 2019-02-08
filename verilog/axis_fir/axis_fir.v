// Author: Erwin Ouyang
// Date  : 4 Feb 2019
// Update: 04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa

`timescale 1ns / 1ps

module axis_fir
    (
        // *** AXI4 clock and reset port ***
        input wire         aclk,
        input wire         aresetn,
        // *** AXI4-stream slave port ***
        output wire        s_axis_tready,
        input wire [31:0]  s_axis_tdata,
        input wire         s_axis_tvalid,
        // *** AXI4-stream master port ***
        input wire         m_axis_tready,
        output wire [31:0] m_axis_tdata,
        output wire        m_axis_tvalid
    );
    
    reg signed [15:0] s_axis_tdata_reg;
    wire signed [31:0] yfir_w;
    
    assign s_axis_tready = m_axis_tready;
    assign m_axis_tvalid = s_axis_tvalid;
    assign m_axis_tdata = {yfir_w[31], yfir_w[31:1]};
    
    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            s_axis_tdata_reg <= 0;
        end
        else
        begin
            s_axis_tdata_reg <= s_axis_tdata[15:0];
        end
    end
    
    fir_dp fir_dp_0
    (
        .clk(aclk),
        .rst_n(aresetn),
        .en(s_axis_tvalid),
        .x(s_axis_tdata_reg),
        .y(yfir_w)
    );
    
endmodule
