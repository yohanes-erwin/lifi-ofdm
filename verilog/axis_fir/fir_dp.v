// Author: Erwin Ouyang
// Date  : 4 Feb 2019
// Update: 04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa

`timescale 1ns / 1ps

module fir_dp
    (
        input  wire               clk,
        input  wire               rst_n,
        input  wire               en,
        input  wire signed [15:0] x,
        output wire signed [31:0] y
    );
    
    wire signed [15:0] a[5:0];
    reg signed [15:0] b[5:0];
    wire signed [31:0] c[5:0];
    wire signed [31:0] d[5:0];
    wire signed [15:0] e[5:0];
    reg signed [31:0] y_i;
    genvar i;
     
    generate
        for (i = 0; i <= 5; i = i+1)
        begin
            fir_pe pe_0_5 (clk, rst_n, en, a[i], b[i], c[i], d[i], e[i]);
            if (i > 0)
            begin
                assign a[i] = e[i-1];
                assign c[i] = d[i-1];  
            end
        end
    endgenerate
    
    assign a[0] = x;
    assign c[0] = 0;
 
    initial
    begin
        // Low pass, FIR, window, kaiser, beta 0.6, 5 tap, fs = 125MHz, fc = 25MHz, scaling = 8192  
        b[0] = 16'b0000000000000000;
        b[1] = 16'b0000010110000001;
        b[2] = 16'b0000101001111111;
        b[3] = 16'b0000101001111111;
        b[4] = 16'b0000010110000001; 
        b[5] = 16'b0000000000000000;
    end
    
    always @(posedge clk)
    begin
        if (!rst_n)
        begin
            y_i <= 0;
        end
        else
        begin
            if (en)
                y_i <= d[5];
        end
    end

    assign y = ((y_i[31]) ? {12'b111111111111, y_i[31:12]} : {12'b000000000000, y_i[31:12]});
    
endmodule
