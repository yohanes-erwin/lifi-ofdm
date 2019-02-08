// Author: Erwin Ouyang
// Date  : 4 Jan 2018
// Update: 11 Mar 2018 - Add configurable guard interval
//         30 Jun 2018 - Porting to RedPitaya
//         30 Agt 2018 - Remove preamble CP
//	       04 Feb 2019 - GitHub first commit, from lifi_ap_functional_daa
//         04 Feb 2019 - Add upsample with factor N = 25, change name to axis_preamble

`timescale 1ns / 1ps

module axis_preamble
    (
        // *** AXI4 clock and reset port ***
        input wire         aclk,
        input wire         aresetn,
        // *** AXI4-stream slave port ***
        output wire        s_axis_tready,
        input wire [31:0]  s_axis_tdata,
        input wire         s_axis_tvalid,
        input wire         s_axis_tlast,
        // *** AXI4-stream master port ***
        input wire         m_axis_tready,
        output wire [31:0] m_axis_tdata,
        output wire        m_axis_tvalid,
        output wire        m_axis_tlast,
        // *** User port ***
        input  wire [7:0]  guard_interval,  // 0 - 216 (0, 72, 144, 216)
        output reg         done_tick
    );
    
    localparam S_READ = 2'h0,
               S_WRITE = 2'h1,
               S_DONE = 2'h2;
    localparam UP_FACTOR = 25;
    
    reg [1:0] _cs, _ns;
    reg [7:0] cnt_rd_cv, cnt_rd_nv;
    reg [8:0] cnt_wr_cv, cnt_wr_nv;
    reg [7:0] cnt_up_cv, cnt_up_nv;    // Upsample counter
    reg m_axis_tlast_cv, m_axis_tlast_nv;
    wire signed [15:0] s_axis_tdata_16bit_w;
    wire signed [15:0] s_axis_tdata_max_checked_w;
    wire signed [13:0] s_axis_tdata_14bit_w;
    reg signed [13:0] tx_mem [0:359];  // 72 preamble + 72 data + 3*72 guard interval
    reg [7:0] gi_reg;
    integer i;
    
    assign s_axis_tready = (_cs == S_READ) ? 1 : 0;
    assign m_axis_tdata = tx_mem[cnt_wr_cv];
    assign m_axis_tvalid = (_cs == S_WRITE) ? 1 : 0;
    assign m_axis_tlast = m_axis_tlast_cv;
    
    // *** Check OFDM data max peak (PAPR reduction) ***
    assign s_axis_tdata_16bit_w = s_axis_tdata[15:0];
    assign s_axis_tdata_max_checked_w = (s_axis_tdata_16bit_w > 8191) ? 8191 : ((s_axis_tdata_16bit_w < -8192) ? -8192 : s_axis_tdata_16bit_w);
    assign s_axis_tdata_14bit_w = s_axis_tdata_max_checked_w[13:0];
    
    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            // *** Preamble value ***
            tx_mem[0] <= -6000;
            tx_mem[1] <= 7731;
            tx_mem[2] <= -2938;
            tx_mem[3] <= 783;
            tx_mem[4] <= -4221;
            tx_mem[5] <= -2280;
            tx_mem[6] <= 1505;
            tx_mem[7] <= 2351; // CP end
            tx_mem[8] <= 4243;
            tx_mem[9] <= 78;
            tx_mem[10] <= -268;
            tx_mem[11] <= 1837;
            tx_mem[12] <= -7845;
            tx_mem[13] <= 2396;
            tx_mem[14] <= 5889;
            tx_mem[15] <= 797;
            tx_mem[16] <= -6000;
            tx_mem[17] <= 7731;
            tx_mem[18] <= -2938;
            tx_mem[19] <= 783;
            tx_mem[20] <= -4221;
            tx_mem[21] <= -2280;
            tx_mem[22] <= 1505;
            tx_mem[23] <= 2351;
            tx_mem[24] <= 4243;
            tx_mem[25] <= 78;
            tx_mem[26] <= -268;
            tx_mem[27] <= 1837;
            tx_mem[28] <= -7845;
            tx_mem[29] <= 2396;
            tx_mem[30] <= 5889;
            tx_mem[31] <= 797;
            tx_mem[32] <= -6000;
            tx_mem[33] <= 7731;
            tx_mem[34] <= -2938;
            tx_mem[35] <= 783;
            tx_mem[36] <= -4221;
            tx_mem[37] <= -2280;
            tx_mem[38] <= 1505;
            tx_mem[39] <= 2351;
            tx_mem[40] <= 4243;
            tx_mem[41] <= 78;
            tx_mem[42] <= -268;
            tx_mem[43] <= 1837;
            tx_mem[44] <= -7845;
            tx_mem[45] <= 2396;
            tx_mem[46] <= 5889;
            tx_mem[47] <= 797;
            tx_mem[48] <= -6000;
            tx_mem[49] <= 7731;
            tx_mem[50] <= -2938;
            tx_mem[51] <= 783;
            tx_mem[52] <= -4221;
            tx_mem[53] <= -2280;
            tx_mem[54] <= 1505;
            tx_mem[55] <= 2351;
            tx_mem[56] <= 4243;
            tx_mem[57] <= 78;
            tx_mem[58] <= -268;
            tx_mem[59] <= 1837;
            tx_mem[60] <= -7845;
            tx_mem[61] <= 2396;
            tx_mem[62] <= 5889;
            tx_mem[63] <= 797;
            tx_mem[64] <= -6000;
            tx_mem[65] <= 7731;
            tx_mem[66] <= -2938;
            tx_mem[67] <= 783;
            tx_mem[68] <= -4221;
            tx_mem[69] <= -2280;
            tx_mem[70] <= 1505;
            tx_mem[71] <= 2351;
            // *** Data ***
            for (i = 72; i <= 143; i = i+1) // 64 135
                tx_mem[i] <= 0;
            // *** Guard interval ***
            for (i = 144; i <= 359; i = i+1) // 136 351
                tx_mem[i] <= 0;
        end
        else
        begin
            // *** OFDM data + CP from IFFT output ***
            if (s_axis_tvalid)
                tx_mem[cnt_rd_cv] <= s_axis_tdata_14bit_w;
        end
    end
    
    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            _cs <= S_READ;
            cnt_rd_cv <= 72;
            cnt_wr_cv <= 0;
            cnt_up_cv <= 0;
            m_axis_tlast_cv <= 0;
            gi_reg <= 0;
        end
        else
        begin
            _cs <= _ns;
            cnt_rd_cv <= cnt_rd_nv;
            cnt_wr_cv <= cnt_wr_nv;
            cnt_up_cv <= cnt_up_nv;
            m_axis_tlast_cv <= m_axis_tlast_nv;
            gi_reg <= guard_interval;
        end
    end

    always @(*)
    begin
        _ns = _cs;
        cnt_rd_nv = cnt_rd_cv;
        cnt_wr_nv = cnt_wr_cv;
        cnt_up_nv = cnt_up_cv;
        m_axis_tlast_nv = m_axis_tlast_cv;
        done_tick = 0;
        case (_cs)
            S_READ:
            begin
                // *** Read IFFT output ***
                if (s_axis_tvalid)
                begin
                    cnt_rd_nv = cnt_rd_cv + 1;
                    if (s_axis_tlast)
                    begin
                        _ns = S_WRITE;
                        cnt_rd_nv = 72;
                    end
                end
            end
            S_WRITE:
            begin
                // *** Write preamble and OFDM symbol ***
                if (m_axis_tready)
                begin
                    cnt_up_nv = cnt_up_cv + 1;
                    if (cnt_up_cv == UP_FACTOR-1)   // Upsampling process
                    begin
                        cnt_up_nv = 0;
                        if (cnt_wr_cv == 143 + gi_reg)
                        begin
                            _ns = S_DONE;
                            cnt_wr_nv = 0;
                            m_axis_tlast_nv = 0;
                        end
                        else
                        begin
                            cnt_wr_nv = cnt_wr_cv + 1;
                            if (cnt_wr_cv == 142 + gi_reg)
                                m_axis_tlast_nv = 1;
                        end
                    end
                end
            end
            S_DONE:
            begin
                // *** Generate done signal ***
                cnt_up_nv = 0;
                _ns = S_READ;
                done_tick = 1;
            end
        endcase
    end
  
endmodule
