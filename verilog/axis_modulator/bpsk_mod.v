// Author: Erwin Ouyang
// Date  : 6 Jan 2018
// Update: 30 Jun 2018 - Porting to RedPitaya
//	       04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa

`timescale 1ns / 1ps

module bpsk_mod
    (
        input wire        data_in,
        output reg [31:0] data_mod,
        output reg [31:0] data_conj
    );

    localparam P1 = 16'b0010110100000000;   // +1*11520 = +11520
    localparam M1 = 16'b1101001100000000;   // -1*11520 = -11520
        
    // *** Constellation ***
    // 1 | 0
    always @(*)
    begin
        // *** Modulated data ***
        case (data_in)
            0: data_mod <= {16'b0, P1};
            1: data_mod <= {16'b0, M1};
            default: data_mod <= 0;
        endcase
        // *** Conjugate data ***
        case (data_in)
            0: data_conj <= {16'b0, P1};
            1: data_conj <= {16'b0, M1};
            default: data_conj <= 0;
        endcase
    end

endmodule
