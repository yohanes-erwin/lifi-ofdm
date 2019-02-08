// Author: Erwin Ouyang
// Date  : 7 Feb 2019
// Update: 07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module fine_detector
    (
        input wire        clk,
        input wire        rst_n,
        input wire        en,
        input wire [31:0] y,
        output wire       trigger_tick
    );
    
    localparam [31:0] C_FINE_THRESHOLD = 32'd200000000;

    reg [7:0] _cs, _ns;
    reg [11:0] cnt_fine_timeout_cv, cnt_fine_timeout_nv;    // Counter for fine peak timeout
    reg [5:0] cnt_wait_cv, cnt_wait_nv;                     // Counter for removing cyclic prefix
    reg trigger_tick_cv, trigger_tick_nv;
    
    assign trigger_tick = trigger_tick_cv;
    
    always @(posedge clk)
    begin
        if (!rst_n)
        begin
            _cs <= 0;
            cnt_fine_timeout_cv <= 0;
            trigger_tick_cv <= 0;
            cnt_wait_cv <= 0;
        end
        else
        begin
            if (en)
            begin 
                _cs <= _ns;
                cnt_fine_timeout_cv <= cnt_fine_timeout_nv;
                trigger_tick_cv <= trigger_tick_nv;
                cnt_wait_cv <= cnt_wait_nv;
            end
        end
    end
    
    always @(*)
    begin
        _ns = _cs;
        cnt_fine_timeout_nv = cnt_fine_timeout_cv;
        cnt_wait_nv = cnt_wait_cv;
        trigger_tick_nv = 0;
        case (_cs)
            0:  // Wait for peak 1
            begin
//                cnt_fine_timeout_nv = cnt_fine_timeout_cv + 1;
//                if (cnt_fine_timeout_cv > 800)  // Timeout
//                begin
//                    _ns = 8;
//                end
                if (en == 1 && y >= C_FINE_THRESHOLD)    // Peak 1
                begin
                    _ns = 1;
                end
            end
            1:  // Peak 1 found
            begin
                cnt_fine_timeout_nv = 0;
                _ns = 2;
            end
            2:  // Wait for peak 2
            begin
//                cnt_fine_timeout_nv = cnt_fine_timeout_cv + 1;
//                if (cnt_fine_timeout_cv > 400)  // Timeout
//                begin
//                    _ns = 8;
//                end
                if (en == 1 && y >= C_FINE_THRESHOLD)    // Peak 2
                begin
                    _ns = 3;
                end
            end
            3:  // Peak 2 found
            begin
                cnt_fine_timeout_nv = 0;
                _ns = 4;
            end
            4:  // Wait for peak 3
            begin
//                cnt_fine_timeout_nv = cnt_fine_timeout_cv + 1;
//                if (cnt_fine_timeout_cv > 400)  // Timeout
//                begin
//                    _ns = 8;
//                end
                if (en == 1 && y >= C_FINE_THRESHOLD)    // Peak 3
                begin
                    _ns = 5;
                end
            end
            5:  // Peak 3 found
            begin
                cnt_fine_timeout_nv = 0;
                _ns = 6;
            end
            6:  // Wait for peak 4
            begin
//                cnt_fine_timeout_nv = cnt_fine_timeout_cv + 1;
//                if (cnt_fine_timeout_cv > 400)  // Timeout
//                begin
//                    _ns = 8;
//                end
                if (en == 1 && y >= C_FINE_THRESHOLD)    // Peak 4
                begin
                    _ns = 7;
                end
            end
            7:  // Peak 4 found
            begin
                cnt_fine_timeout_nv = 0;
                _ns = 8;                                        
            end
            8:
            begin
                cnt_wait_nv = cnt_wait_cv + 1;
                if (cnt_wait_cv == 4)
                begin
                    trigger_tick_nv = 1;
                    cnt_wait_nv = 0;
                    _ns = 0;
                end
            end
            9:
            begin
                _ns = 0;
            end
        endcase
    end

endmodule
