// Author: Erwin Ouyang
// Date  : 11 Mar 2018
// Update: 26 Sep 2018 - Porting to RedPitaya
//         07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

module baud_gen
    (
        input  wire        clk,
        input  wire        rst_n,
        input  wire [15:0] mod_m,
        output wire        btick
    );
    
    reg[15:0] cnt_cv;
    wire[15:0] cnt_nv;
    
    always @(posedge clk)
    begin
        if (!rst_n)
            cnt_cv <= 0;
        else
            cnt_cv <= cnt_nv;
    end
         
    assign cnt_nv = (cnt_cv == (mod_m-1)) ? 0 : cnt_cv + 1;
    assign btick = (cnt_cv == (mod_m-1)) ? 1 : 0;
    
endmodule
