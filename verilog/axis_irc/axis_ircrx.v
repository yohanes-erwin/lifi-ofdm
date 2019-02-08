// Author: Erwin Ouyang
// Date  : 11 Mar 2018
// Update: 26 Sep 2018 - Porting to RedPitaya
//         07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module axis_ircrx
    #(
        parameter C_DATA_BIT = 8,       // Number of data bits
                  C_STOP_TICK = 16      // Number of ticks for stop bits
    )
    (
        // *** Clock and reset signals ***
        input  wire        aclk,
        input  wire        aresetn,
        // *** AXI4-stream master signals ***
        input  wire        m_axis_tready,
        output wire [7:0]  m_axis_tdata,
        output wire        m_axis_tvalid,
        // *** IRC signals ***
        input  wire [15:0] mod_m,
        input  wire        rx
    );
    
    localparam [2:0] S_IDLE = 3'b000,
                     S_START = 3'b001,
                     S_DATA = 3'b010,
                     S_STOP = 3'b011,
                     S_WR_STREAM = 3'b100;
    
    reg [2:0] rx_cs, rx_ns;
    reg [3:0] cnt_btick_cv, cnt_btick_nv;
    reg [2:0] cnt_bit_cv, cnt_bit_nv;
    reg [7:0] data_cv, data_nv;
    wire btick;

    baud_gen baud_gen_0
    (
        .clk(aclk),
        .rst_n(aresetn),
        .mod_m(8),
        .btick(btick)
    );

    assign m_axis_tdata = data_cv;
    assign m_axis_tvalid = (rx_cs == S_WR_STREAM) ? 1 : 0;

    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            rx_cs <= S_IDLE;
            cnt_btick_cv <= 0;
            cnt_bit_cv <= 0;
            data_cv <= 0;
        end
        else
        begin
            rx_cs <= rx_ns;
            cnt_btick_cv <= cnt_btick_nv;
            cnt_bit_cv <= cnt_bit_nv;
            data_cv <= data_nv;
        end
    end

    always @(*)
    begin
        rx_ns = rx_cs;
        cnt_btick_nv = cnt_btick_cv;
        cnt_bit_nv = cnt_bit_cv;
        data_nv = data_cv;
        case (rx_cs)
            S_IDLE:
            begin
                if (!rx)
                begin
                    rx_ns = S_START;
                    cnt_btick_nv = 0;
                end
            end
            S_START:
            begin
                if (btick)
                begin
                    if (cnt_btick_cv == 7)
                    begin
                        rx_ns = S_DATA;
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
                if (btick)
                begin
                    if (cnt_btick_cv == 15)
                    begin
                        cnt_btick_nv = 0;
                        data_nv = {rx, data_cv[7:1]};
                        if (cnt_bit_cv == (C_DATA_BIT-1))    // 8 data bits
                            rx_ns = S_STOP;
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
                if (btick)
                begin
                    if (cnt_btick_cv == (C_STOP_TICK-1))     // 1 stop bit
                    begin
                        rx_ns = S_WR_STREAM;
                    end
                    else
                    begin
                        cnt_btick_nv = cnt_btick_cv + 1;
                    end
                end
            end
            S_WR_STREAM:
            begin
                if (m_axis_tready)
                begin
                    rx_ns = S_IDLE;
                end
            end
        endcase
    end
    
endmodule
