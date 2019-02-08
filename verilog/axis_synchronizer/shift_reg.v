// Author: Erwin Ouyang
// Date  : 7 Feb 2019
// Update: 07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

// ### Description #############################################################
// A shift register for DC metric. From OFDM Baseband Receiver
// Design for wireless Communications" by Tzi-Dar Chiueh and Pei-Yun Tsai,
// pp.96, Figure 5.7.

`timescale 1ns / 1ps

module shift_reg
    (
        input wire         clk,
        input wire         rst_n,
        input wire [31:0]  d,
        output wire [31:0] q 
    );

    // Number of preamble * oversampling factor = 16 * 25 = 400
    localparam DEPTH = 400;
    
    reg [31:0] fifo [0:DEPTH-1];
    integer i;
    
    always @(posedge clk)
    begin
        if (!rst_n)
        begin
            for (i = 0; i <= DEPTH-1; i = i+1)
                fifo[i] <= 0;
        end
        else
        begin
            for (i = 1; i <= DEPTH-1; i = i+1)
                fifo[i] <= fifo[i-1];
            fifo[0] <= d;
        end
    end

    assign q = fifo[DEPTH-1];
    
endmodule
