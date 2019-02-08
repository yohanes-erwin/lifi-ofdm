// Author: Erwin Ouyang
// Date  : 15 Jan 2018
// Update: 01 Jul 2018 - Porting to RedPitaya
//	       04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa

`timescale 1ns / 1ps

module qpsk_demod
    (
        input wire signed [22:0] data_in_re,
        input wire signed [22:0] data_in_im,
        output wire [1:0]        data_demod
    );

    // *** Constellation ***
    // 10 | 00
    // ---|---
    // 11 | 01
    assign data_demod[1] = (data_in_re < 0) ? 1 : 0;
    assign data_demod[0] = (data_in_im < 0) ? 1 : 0;
    
endmodule
