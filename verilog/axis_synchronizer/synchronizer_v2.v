// Author: Erwin Ouyang
// Date  : 7 Feb 2019
// Update: 07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module synchronizer_v2
    (
        // *** AXI4 clock and reset port ***
        input wire         aclk,
        input wire         aresetn,
        // *** AXI4-stream slave port ***
        output wire        s_axis_tready,
        input wire [31:0]  s_axis_tdata,
        input wire         s_axis_tvalid,
        // *** AXI4-stream master port ***
        input wire         m_axis_tready,
        output wire [31:0] m_axis_tdata,
        output wire        m_axis_tvalid,
        output wire        m_axis_tlast,
        // *** BRAM ***
        output wire        clka,
        output wire        clkb,
        output wire        rsta,
        output wire        rstb,
        output wire        ena,
        output wire        enb,
        output wire [12:0] addra,
        output wire [12:0] addrb,
        output wire [31:0] dina,
        output wire [31:0] dinb,
        input wire [31:0]  douta,
        input wire [31:0]  doutb,
        output wire        wea,
        output wire        web
    );

    wire signed [31:0] adc_data;
    wire signed [31:0] dc_metric_0_out;
    wire [12:0]addr_counter_0_addra;
    wire [12:0]addr_counter_0_addrb;
    wire [12:0] sync_ctl_0_addrb_load;
    wire sync_ctl_0_addrb_load_en;
    wire sync_ctl_0_reb;
    wire sync_ctl_0_valid_downsamp;
    wire sync_ctl_0_valid_final;
    wire sync_ctl_0_last_final;
    wire sync_ctl_0_wea;
    wire [31:0] fine_corr_dp_0_y;
    wire fine_detector_0_trigger_tick;
    
    assign adc_data = {{18{s_axis_tdata[13]}}, s_axis_tdata[13:0]};     // Sign extentsion of ADC data
    
    assign clka = aclk;
    assign clkb = aclk;
    assign rsta = ~aresetn;
    assign rstb = ~aresetn;
    assign ena = 1;
    assign enb = 1;
    assign addra = addr_counter_0_addra;
    assign addrb = addr_counter_0_addrb;
    assign dina = adc_data;
    assign dinb = 32'h0;
    assign wea = sync_ctl_0_wea;
    assign web = 0; 
    
    assign s_axis_tready = 1;
    
    assign m_axis_tdata = doutb;
    assign m_axis_tvalid = sync_ctl_0_valid_final;
    assign m_axis_tlast = sync_ctl_0_last_final; 

    dc_metric dc_metric_0
    (
        .clk(aclk),
        .rst_n(aresetn),
        .in(adc_data),
        .out(dc_metric_0_out)
    );
    
    addr_counter addr_counter_0
    (
        .clk(aclk),
        .rst_n(aresetn),
        .wea(sync_ctl_0_wea),
        .addra(addr_counter_0_addra),
        .reb(sync_ctl_0_reb),
        .addrb_load_en(sync_ctl_0_addrb_load_en),
        .addrb_load(sync_ctl_0_addrb_load),
        .addrb(addr_counter_0_addrb)
    );      
    
    fine_corr_dp fine_corr_dp_0
    (
        .clk(aclk),
        .rst_n(aresetn),
        .en(sync_ctl_0_valid_downsamp),
        .x(doutb[13:0]),
        .y(fine_corr_dp_0_y)
    );
    
    fine_detector fine_detector_0
    (
        .clk(aclk),
        .rst_n(aresetn),
        .en(sync_ctl_0_valid_downsamp),
        .y(fine_corr_dp_0_y),
        .trigger_tick(fine_detector_0_trigger_tick)
    );
            
    sync_ctl sync_ctl_0
    (
        .clk(aclk),
        .rst_n(aresetn),
        .addra(addr_counter_0_addra),
        .wea(sync_ctl_0_wea),
        .addrb(addr_counter_0_addrb),
        .reb(sync_ctl_0_reb),
        .addrb_load_en(sync_ctl_0_addrb_load_en),
        .addrb_load(sync_ctl_0_addrb_load),
        .dc_metric_i(dc_metric_0_out),
        .valid_downsamp(sync_ctl_0_valid_downsamp),
        .fine_trigger(fine_detector_0_trigger_tick),
        .valid_final(sync_ctl_0_valid_final),
        .last_final(sync_ctl_0_last_final)
    );
    
endmodule
