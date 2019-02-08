// Author: Erwin Ouyang
// Date  : 11 Mar 2018
// Update: 26 Sep 2018 - Porting to RedPitaya
//         07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module axis_irctx
    #(
        parameter C_DATA_BIT = 8,       // Number of data bits
                  C_STOP_TICK = 16      // Number of ticks for stop bits
    )
    (
        // *** Clock and reset signals ***
        input  wire        aclk,
        input  wire        aresetn,
        // *** AXI4-stream slave signals ***
        output wire        s_axis_tready,
        input  wire [7:0]  s_axis_tdata,
        input  wire        s_axis_tvalid,
        // *** IRC signals ***
        input  wire [15:0] mod_m,
        input  wire        mod_38khz_en,
        output wire        tx_mod
    );

    localparam [1:0] S_IDLE = 2'b000,
                     S_START = 2'b001,
                     S_DATA = 2'b010,
                     S_STOP = 2'b011;
    
    reg [1:0] tx_cs, tx_ns;
    reg [3:0] cnt_btick_cv, cnt_btick_nv;
    reg [2:0] cnt_bit_cv, cnt_bit_nv;
    reg [7:0] data_cv, data_nv;
    reg tx_cv, tx_nv;
    wire btick;
    
    baud_gen baud_gen_0
    (
        .clk(aclk),
        .rst_n(aresetn),
        .mod_m(8),
        .btick(btick)
    );
       
    mod_38khz mod_38khz_0
    (
        .clk(aclk),
        .rst_n(aresetn),
        .mod_38khz_en(mod_38khz_en),
        .tx(tx_cv),
        .tx_mod(tx_mod)
    );

    assign s_axis_tready = (tx_cs == S_IDLE) ? 1 : 0;
        
    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            tx_cs <= S_IDLE;
            cnt_btick_cv <= 0;
            cnt_bit_cv <= 0;
            data_cv <= 0;
            tx_cv <= 1;
        end
        else
        begin
            tx_cs <= tx_ns;
            cnt_btick_cv <= cnt_btick_nv;
            cnt_bit_cv <= cnt_bit_nv;
            data_cv <= data_nv;
            tx_cv <= tx_nv;
        end
    end

    always @(*)
    begin
        tx_ns = tx_cs;
        cnt_btick_nv = cnt_btick_cv;
        cnt_bit_nv = cnt_bit_cv;
        data_nv = data_cv;
        tx_nv = tx_cv;   
        case (tx_cs)
            S_IDLE:
            begin
                tx_nv = 1;
                if (s_axis_tvalid)
                begin
                    tx_ns = S_START;
                    cnt_btick_nv = 0;
                    data_nv = s_axis_tdata;
                end
            end
            S_START:
            begin
                tx_nv = 0;
                if (btick)
                begin
                    if (cnt_btick_cv == 15)
                    begin
                        tx_ns = S_DATA;
                        cnt_btick_nv = 0;
                        cnt_bit_nv = 0;
                    end  
                    else
                    begin
                        cnt_btick_nv = cnt_btick_cv + 1;
                    end
                end
            end             
            S_DATA:
            begin
                tx_nv = data_cv[0];
                if (btick)
                begin
                    if (cnt_btick_cv == 15)
                    begin
                        cnt_btick_nv = 0;
                        data_nv = data_cv >> 1;
                        if (cnt_bit_cv == (C_DATA_BIT-1))     // 8 data bits
                            tx_ns = S_STOP;
                        else
                            cnt_bit_nv = cnt_bit_cv + 1;
                    end  
                    else
                    begin
                        cnt_btick_nv = cnt_btick_cv + 1;
                    end
                end
            end
            S_STOP:
            begin
                tx_nv = 1;
                if (btick)
                begin
                    if (cnt_btick_cv == (C_STOP_TICK-1))      // 1 stop bit
                    begin
                        tx_ns = S_IDLE;
                    end
                    else
                    begin
                        cnt_btick_nv = cnt_btick_cv + 1;
                    end
                end
            end
        endcase
    end
  
endmodule
