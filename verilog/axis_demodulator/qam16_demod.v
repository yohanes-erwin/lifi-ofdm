// Author: Erwin Ouyang
// Date  : 15 Jan 2018
// Update: 01 Jul 2018 - Porting to RedPitaya
//	       04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa


`timescale 1ns / 1ps

module qam16_demod
    (
        input wire signed [22:0] data_in_re,
        input wire signed [22:0] data_in_im,
        output wire [3:0]        data_demod
    );
    
//    localparam P2 = 16'b0010101110111000;   // +2*5761 = +11522
    localparam P2 = 16'b0001110101001100;   // +7500
    
    wire signed [22:0] abs_data_in_re, abs_data_in_im;
    
    assign abs_data_in_re = (data_in_re[22]) ? -data_in_re : data_in_re;
    assign abs_data_in_im = (data_in_im[22]) ? -data_in_im : data_in_im;
    
    // *** Constellation ***
    // 1101 1001 | 0001 0101
    // 1100 1000 | 0000 0100
    // ----------|----------
    // 1110 1010 | 0010 0110
    // 1111 1011 | 0011 0111  
    assign data_demod[3] = (data_in_re < 0) ? 1 : 0;
    assign data_demod[2] = (abs_data_in_re > P2) ? 1 : 0;
    assign data_demod[1] = (data_in_im < 0) ? 1 : 0;
    assign data_demod[0] = (abs_data_in_im > P2) ? 1 : 0;
    
endmodule
