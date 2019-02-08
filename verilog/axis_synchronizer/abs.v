// Author: Erwin Ouyang
// Date  : 7 Feb 2019
// Update: 07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module abs
    (
        input wire [31:0] in,
        output wire [31:0] out 
    );
    
    assign out = (in[31]) ? -in : in;
    
endmodule
