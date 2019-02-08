// Author: Erwin Ouyang
// Date  : 7 Feb 2019
// Update: 07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

// ### Description #############################################################
// A single register for triggering ADC from DAC.
// DAC -> trigger_register_0 -> trigger_register_1 -> ADC

`timescale 1ns / 1ps

module trigger_register
    #(
        parameter integer DATA_WIDTH = 8
    )
    (
        input wire                  clk,
        input wire                  rst_n,
        input wire[DATA_WIDTH-1:0]  d,
        output reg [DATA_WIDTH-1:0] q
    );
    
    always @(posedge clk)
    begin
        if (!rst_n)
        begin
            q <= 0;
        end
        else
        begin
            q <= d;
        end
    end
    
endmodule
