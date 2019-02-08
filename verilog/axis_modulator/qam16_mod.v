// Author: Erwin Ouyang
// Date  : 5 Jan 2018
// Update: 30 Jun 2018 - Porting to RedPitaya
//	       04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa

`timescale 1ns / 1ps

module qam16_mod
    (
        input wire [3:0]  data_in,
        output reg [31:0] data_mod,
        output reg [31:0] data_conj
    );
    
//    localparam P3 = 16'b0100000110010101;   // +3*5596 = +16789
//    localparam P1 = 16'b0001010111011100;   // +1*5596 = +5596
//    localparam M1 = 16'b1110101000100100;   // -1*5596 = -5596
//    localparam M3 = 16'b1011111001101011;   // -3*5596 = -16789
    localparam P3 = 16'b0011100110010110;   // +3*4914 = +14742
    localparam P1 = 16'b0001001100110010;   // +1*4914 = +4914
    localparam M1 = 16'b1110110011001110;   // -1*4914 = -4914
    localparam M3 = 16'b1100011001101010;   // -3*4914 = -14742
    
    // *** Constellation ***
    // 13  9 |  1  5
    // 12  8 |  0  4
    // ------|------
    // 14 10 |  2  6
    // 15 11 |  3  7
    always @(*)
    begin
        // *** Modulated data ***
        case (data_in)
            0: data_mod <= {P1, P1};
            1: data_mod <= {P3, P1};
            2: data_mod <= {M1, P1};
            3: data_mod <= {M3, P1};
            4: data_mod <= {P1, P3};
            5: data_mod <= {P3, P3};
            6: data_mod <= {M1, P3};
            7: data_mod <= {M3, P3};
            8: data_mod <= {P1, M1};
            9: data_mod <= {P3, M1};
            10: data_mod <= {M1, M1};
            11: data_mod <= {M3, M1};
            12: data_mod <= {P1, M3};
            13: data_mod <= {P3, M3};
            14: data_mod <= {M1, M3};
            15: data_mod <= {M3, M3};
            default: data_mod <= 0;
        endcase
        // *** Conjugate data ***
        case (data_in)
            0: data_conj <= {M1, P1};
            1: data_conj <= {M3, P1};
            2: data_conj <= {P1, P1};
            3: data_conj <= {P3, P1};
            4: data_conj <= {M1, P3};
            5: data_conj <= {M3, P3};
            6: data_conj <= {P1, P3};
            7: data_conj <= {P3, P3};
            8: data_conj <= {M1, M1};
            9: data_conj <= {M3, M1};
            10: data_conj <= {P1, M1};
            11: data_conj <= {P3, M1};
            12: data_conj <= {M1, M3};
            13: data_conj <= {M3, M3};
            14: data_conj <= {P1, M3};
            15: data_conj <= {P3, M3};
            default: data_conj <= 0;
        endcase
    end
    
endmodule
