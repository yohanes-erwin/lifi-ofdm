// Author: Erwin Ouyang
// Date  : 15 Jan 2018
// Update: 01 Jul 2018 - Porting to RedPitaya
//	       04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa


`timescale 1ns / 1ps

module bpsk_demod
    (
        input wire signed [22:0] data_in_re,
        output wire              data_demod
    );

    // *** Constellation ***
    // 1 | 0
    assign data_demod = (data_in_re < 0) ? 1 : 0;
       
endmodule
