// Author: Erwin Ouyang
// Date  : 30 Jan 2018
// Update: 01 Jul 2018 - Porting to RedPitaya
//         07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module fine_corr_pe
    (
        input  wire               clk,
        input  wire               rst_n,
        input  wire               en,
        input  wire signed [13:0] a, b,
        input  wire signed [31:0] c,
        output reg signed [31:0]  d,
        output wire signed [13:0] e
    );
    
    wire signed [27:0] ab;
    wire signed [31:0] abc;
    
    assign ab = a * b;
    assign abc = ab + c;
    assign e = a;
    
    always @(posedge clk)
    begin
        if (!rst_n)
        begin
            d <= 0;
        end
        else
        begin
            if (en)
                d <= abc;
        end
    end
    
endmodule
