// Author: Erwin Ouyang
// Date  : 7 Feb 2019
// Update: 07 Feb 2019 - Change to pipeline architecture
//		   07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module dc_metric
    (
        input wire         clk,
        input wire         rst_n,
        input wire [31:0]  in,
        output wire [31:0] out
    );
    
    wire [31:0] in_pip, reg0_out_w, reg1_out_w, reg2_out_w;
    wire [63:0] mult_out_tmp;
    reg [63:0] mult_out_tmp_pip_reg;
    wire [31:0] mult_out_w, mult_out_w_pip, add_out_w, sub_out_w, sub_out_w_pip;
    
	// *** The circuit is from "OFDM Baseband Receiver Design for wireless Communications"
	// by Tzi-Dar Chiueh and Pei-Yun Tsai, pp.96, Figure 5.7 ***
    shift_reg reg0
    (
        .clk(clk),
        .rst_n(rst_n),
        .d(in_pip),
        .q(reg0_out_w)
    );
    shift_reg reg1
    (
        .clk(clk),
        .rst_n(rst_n),
        .d(mult_out_w_pip),
        .q(reg1_out_w)
    );
    single_reg reg2
    (
        .clk(clk),
        .rst_n(rst_n),
        .d(sub_out_w),
        .q(reg2_out_w)
    );
    abs abs0
    (
        .in(sub_out_w_pip),
        .out(out)
    );

	// *** 4-stage pipeline architecture proposed by me ***
    single_reg reg_pip0
    (
        .clk(clk),
        .rst_n(rst_n),
        .d(in),
        .q(in_pip)
    );
    always @(posedge clk)
    begin
        if (!rst_n)
        begin
            mult_out_tmp_pip_reg <= 0;
        end
        else
        begin
            mult_out_tmp_pip_reg <= mult_out_tmp;
        end
    end
    single_reg reg_pip2
    (
        .clk(clk),
        .rst_n(rst_n),
        .d(mult_out_w),
        .q(mult_out_w_pip)
    );
    single_reg reg_pip3
    (
        .clk(clk),
        .rst_n(rst_n),
        .d(sub_out_w),
        .q(sub_out_w_pip)
    );
    
    assign mult_out_tmp = in_pip * reg0_out_w;
    assign mult_out_w = mult_out_tmp_pip_reg[63:32];
    assign add_out_w = mult_out_w_pip + reg2_out_w;
    assign sub_out_w = add_out_w - reg1_out_w;
    
endmodule
