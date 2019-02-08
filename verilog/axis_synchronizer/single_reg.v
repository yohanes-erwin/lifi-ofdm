// Author: Erwin Ouyang
// Date  : 7 Feb 2019
// Update: 07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

// ### Description #############################################################
// A single register for DC metric. From OFDM Baseband Receiver
// Design for wireless Communications" by Tzi-Dar Chiueh and Pei-Yun Tsai,
// pp.96, Figure 5.7.

`timescale 1ns / 1ps

module single_reg
    (
        input wire        clk,
        input wire        rst_n,
        input wire [31:0] d,
        output reg [31:0] q 
    );

    always @(posedge clk)
    begin
        if (!rst_n)
            q <= 0;
        else
            q <= d;
    end

endmodule
