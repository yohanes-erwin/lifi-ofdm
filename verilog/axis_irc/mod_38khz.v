// Author: Erwin Ouyang
// Date  : 11 Mar 2018
// Update: 26 Sep 2018 - Porting to RedPitaya
//         07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

module mod_38khz
    (
        input  wire clk,
        input  wire rst_n,
        input  wire mod_38khz_en,
        input  wire tx,
        output wire tx_mod
    );
    
    localparam C_CNT_MAX = 1315;    // 100 MHz / 38 kHz / 2 = 1315
    reg[11:0] cnt_cv;
    wire[11:0] cnt_nv;
    reg clk_38khz_cv;
    wire clk_38khz_nv;
    
    always @(posedge clk)
    begin
        if (!rst_n)
        begin
            cnt_cv <= 0;
            clk_38khz_cv <= 0;
        end
        else
        begin
            cnt_cv <= cnt_nv;
            clk_38khz_cv <= clk_38khz_nv;
        end   
    end

    assign cnt_nv = (cnt_cv == (C_CNT_MAX-1)) ? 0 : cnt_cv + 1;
    assign clk_38khz_nv = (cnt_cv == (C_CNT_MAX-1)) ? ~clk_38khz_cv : clk_38khz_cv;
    assign tx_mod = (!mod_38khz_en) ? (tx) : ((!tx) ? clk_38khz_cv : 0);

endmodule
