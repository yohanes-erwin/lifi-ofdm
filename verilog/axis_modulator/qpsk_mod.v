// Author: Erwin Ouyang
// Date  : 6 Jan 2018
// Update: 30 Jun 2018 - Porting to RedPitaya
//	       04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa

`timescale 1ns / 1ps

module qpsk_mod
    (
        input wire [1:0]  data_in,
        output reg [31:0] data_mod,
        output reg [31:0] data_conj
    );

    localparam P1 = 16'b0010110100000000;   // +1*11520 = +11520
    localparam M1 = 16'b1101001100000000;   // -1*11520 = -11520
    
    // *** Constellation ***
    // 2 | 0
    // --|--
    // 3 | 1
    always @(*)
    begin
        // *** Modulated data ***
        case (data_in)
            0: data_mod <= {P1, P1};
            1: data_mod <= {M1, P1};
            2: data_mod <= {P1, M1};
            3: data_mod <= {M1, M1};
            default: data_mod <= 0;
        endcase
        // *** Conjugate data ***
        case (data_in)
            0: data_conj <= {M1, P1};
            1: data_conj <= {P1, P1};
            2: data_conj <= {M1, M1};
            3: data_conj <= {P1, M1};
            default: data_conj <= 0;
        endcase      
    end
    
endmodule
