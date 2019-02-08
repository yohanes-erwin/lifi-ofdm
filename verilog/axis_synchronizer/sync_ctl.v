// Author: Erwin Ouyang
// Date  : 7 Feb 2019
// Update: 07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module sync_ctl
    (
        input wire         clk,
        input wire         rst_n,
        // *** BRAM ***
        input wire [12:0]  addra,
        output wire        wea,
        input wire [12:0]  addrb,
        // *** Address counter ***
        output wire        reb,
        output wire        addrb_load_en,
        output wire [12:0] addrb_load,
        // *** Coarse correlation ***
        input wire [31:0]  dc_metric_i,
        // *** Downsample ***
        output wire        valid_downsamp,
        // *** Fine correlation ***
        input wire         fine_trigger,
        output wire        valid_final,
        output wire        last_final
    );

    localparam [31:0] C_DC_THRESHOLD = 32'd900000,
                      C_FINE_THRESHOLD = 32'd210000000;
    localparam [4:0] C_N_DOWNSAMP = 5'd25; 
    
    reg [7:0] _cs, _ns;
    // *** BRAM ***
    reg wea_cv, wea_nv;
    // *** Address counter ***
    reg reb_cv, reb_nv;
    reg addrb_load_en_cv, addrb_load_en_nv;
    // *** Coarse correlation ***
    reg dc_trigger_cv, dc_trigger_nv;
    reg [12:0] addr_at_start_sym_cv, addr_at_start_sym_nv;
    reg [12:0] sym_len_cv, sym_len_nv;
    // *** Downsample ***
    reg valid_downsamp_cv, valid_downsamp_nv;
    // *** Fine correlation ***
    reg valid_sync_cv, valid_sync_nv;
    reg last_cv, last_nv;
    // *** Counters ***
    reg [11:0] cnt_plateau_cv, cnt_plateau_nv;              // Counter plateau of DC metric
    reg [11:0] cnt_wait_sym_cv, cnt_wait_sym_nv;            // Counter for wait until one symbol is inside the FIFO seek
    reg [4:0] cnt_downsamp_cv, cnt_downsamp_nv;             // Counter for downsampling
    reg [5:0] cnt_sample_for_fft_cv, cnt_sample_for_fft_nv; // Counter for counting number of FFT input (64 sample)
    
    assign wea = wea_cv;
    assign reb = reb_cv;
    assign addrb_load_en = addrb_load_en_cv;
    assign addrb_load = addr_at_start_sym_cv;
    assign valid_downsamp = valid_downsamp_cv;
    assign valid_final = valid_downsamp_cv & valid_sync_cv & reb_cv;
    assign last_final = last_cv & valid_final;
    
    always @(posedge clk)
    begin
        if (!rst_n)
        begin
            _cs <= 0;
            wea_cv <= 1;
            reb_cv <= 0;
            addrb_load_en_cv <= 0;
            dc_trigger_cv <= 0;
            addr_at_start_sym_cv <= 0;
            sym_len_cv <= 0;
            valid_downsamp_cv <= 20;
            valid_sync_cv <= 0;
            cnt_plateau_cv <= 0;
            cnt_wait_sym_cv <= 0;
            cnt_downsamp_cv <= 0;
            cnt_sample_for_fft_cv <= 0;
            last_cv <= 0;
        end
        else
        begin
            _cs <= _ns;
            wea_cv <= wea_nv;
            reb_cv <= reb_nv;
            addrb_load_en_cv <= addrb_load_en_nv;
            dc_trigger_cv <= dc_trigger_nv;
            addr_at_start_sym_cv <= addr_at_start_sym_nv;
            sym_len_cv <= sym_len_nv;
            valid_downsamp_cv <= valid_downsamp_nv;
            valid_sync_cv <= valid_sync_nv;
            cnt_plateau_cv <= cnt_plateau_nv;
            cnt_wait_sym_cv <= cnt_wait_sym_nv;
            cnt_downsamp_cv <= cnt_downsamp_nv;
            cnt_sample_for_fft_cv <= cnt_sample_for_fft_nv;
            last_cv <= last_nv;
        end
    end
    
    always @(*)
    begin
        _ns = _cs;
        wea_nv = wea_cv;
        reb_nv = reb_cv;
        addrb_load_en_nv = 0;
        dc_trigger_nv = 0;
        addr_at_start_sym_nv = addr_at_start_sym_cv;
        sym_len_nv = sym_len_cv;
        valid_downsamp_nv = 0;
        valid_sync_nv = valid_sync_cv;
        cnt_plateau_nv = cnt_plateau_cv;
        cnt_wait_sym_nv = cnt_wait_sym_cv;
        cnt_downsamp_nv = cnt_downsamp_cv;
        cnt_sample_for_fft_nv = cnt_sample_for_fft_cv;
        last_nv = last_cv;
        case (_cs)
            0:  // Idle, wait for DC metric to be larger than threshold
            begin
                // If coarse correlation (DC metirc) is larger than threshold
                if (dc_metric_i >= C_DC_THRESHOLD)
                    _ns = 1;
            end
            1:  // Count DC metric duration, and also check if DC metric is drop below threshold
            begin   
                if (dc_metric_i < C_DC_THRESHOLD)
                begin
                    // If below threshold, then reset
                    cnt_plateau_nv = 0;
                    _ns = 0;
                end
                else
                begin
                    // Increment plateau counter
                    cnt_plateau_nv = cnt_plateau_cv + 1;
                    if (cnt_plateau_cv >= 1000)     // Plateatu duration: 8us/8ns = 1000 clock
                    begin
                        cnt_plateau_nv = 0;
                        _ns = 2;
                    end
                end
            end
            2:  // Trigger coarse correlation
            begin
                dc_trigger_nv = 1;
                addr_at_start_sym_nv = addra - 1710;
                sym_len_nv = addra - 1710 + 3650;
                _ns = 3;
            end
            3:  // Wait until one complete symbol is in memory
            begin
                cnt_wait_sym_nv = cnt_wait_sym_cv + 1;
                if (cnt_wait_sym_cv == 2250)    // 18us/8ns = 2250 clock
                begin
                    wea_nv = 0;                 // Disable write ADC data to BRAM
                    cnt_wait_sym_nv = 0;
                    _ns = 4;
                end
            end
            4:  // Load address at start symbol to read address counter
            begin
                addrb_load_en_nv = 1;
                _ns = 5;
            end
            5:  //  Read data from BRAM
            begin
                reb_nv = 1;                                 // Enable read address counter
                cnt_downsamp_nv = cnt_downsamp_cv + 1;      // Increment downsample counter
                if (cnt_downsamp_cv == (C_N_DOWNSAMP-1))
                begin
                    cnt_downsamp_nv = 0;
                    valid_downsamp_nv = 1;                  // Pick up a sample
                end
                if (fine_trigger)
                begin
                    valid_sync_nv = 1;
                end
                if (valid_sync_cv && valid_downsamp_cv)
                begin
                    cnt_sample_for_fft_nv = cnt_sample_for_fft_cv + 1;          // Count number of sample up to 64
                    if (cnt_sample_for_fft_cv == 62 && valid_downsamp_cv == 1)  // At the last sample
                    begin
                        last_nv = 1;
                    end
                    if (cnt_sample_for_fft_cv == 63 && valid_downsamp_cv == 1)
                    begin
                        last_nv = 0;
                        valid_sync_nv = 0;
                        cnt_sample_for_fft_nv = 0;
                    end
                end
                if (addrb == sym_len_cv)
                begin
                    reb_nv = 0;
                    valid_sync_nv = 0;
                    
                    cnt_downsamp_nv = 20;
                    wea_nv = 1;
                    
                    _ns = 0;
                end
            end
        endcase
    end
    
endmodule
