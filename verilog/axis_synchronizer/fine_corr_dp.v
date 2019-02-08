// Author: Erwin Ouyang
// Date  : 30 Jan 2018
// Update: 01 Jul 2018 - Porting to RedPitaya
//         07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module fine_corr_dp
    (
        input  wire               clk,
        input  wire               rst_n,
        input  wire               en,
        input  wire signed [13:0] x,
        output wire signed [31:0] y
    );
    
    wire signed [13:0] a[15:0];
    wire signed [13:0] b[15:0];
    wire signed [31:0] c[15:0];
    wire signed [31:0] d[15:0];
    wire signed [13:0] e[15:0];
    reg signed [31:0] y_i;
    genvar i;
     
    generate
        for (i = 0; i <= 15; i = i+1)
        begin
            fine_corr_pe pe_0_15 (clk, rst_n, en, a[i], b[i], c[i], d[i], e[i]);
            if (i > 0)
            begin
                assign a[i] = e[i-1];
                assign c[i] = d[i-1];  
            end
        end
    endgenerate
    
    assign a[0] = x;
    assign c[0] = 0;
    
    always @(posedge clk)
    begin
        if (!rst_n)
        begin
            y_i <= 0;
        end
        else
        begin
            if (en)
                y_i <= d[15];
        end
    end

    assign b[0] = 4243;
    assign b[1] = 78;
    assign b[2] = -268;
    assign b[3] = 1837;
    assign b[4] = -7845;
    assign b[5] = 2396;
    assign b[6] = 5889;
    assign b[7] = 797;
    assign b[8] = -6000;
    assign b[9] = 7731;
    assign b[10] = -2938;
    assign b[11] = 783;
    assign b[12] = -4221;
    assign b[13] = -2280;
    assign b[14] = 1505;
    assign b[15] = 2351;
    
    assign y = (y_i[31]) ? -y_i : y_i;
    
endmodule
