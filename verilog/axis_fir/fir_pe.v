// Author: Erwin Ouyang
// Date  : 4 Feb 2019
// Update: 04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa

`timescale 1ns / 1ps

module fir_pe
    (
        input  wire               clk,
        input  wire               rst_n,
        input  wire               en,
        input  wire signed [15:0] a, b,
        input  wire signed [31:0] c,
        output reg signed [31:0]  d,
        output wire signed [15:0] e
    );
    
    wire signed [31:0] ab;
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
