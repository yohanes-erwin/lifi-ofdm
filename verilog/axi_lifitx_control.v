// Author: Erwin Ouyang
// Date  : 23 Feb 2018
// Update: 11 Mar 2018 - Add guard interval control register
//         15 Jul 2018 - Porting to RedPitaya
//	       28 Jan 2019 - GitHub first commit

`timescale 1ns / 1ps

module axi_lifitx_control
    (
	    // *** AXI4 clock and reset port ***
		input  wire        aclk,
		input  wire        aresetn,
        // *** AXI4-lite slave port ***
        output wire        s_axi_awready,
		input  wire [31:0] s_axi_awaddr,
		input  wire        s_axi_awvalid,
		output wire        s_axi_wready,
		input  wire [3:0]  s_axi_wstrb,
		input  wire [31:0] s_axi_wdata,
		input  wire        s_axi_wvalid,
		input  wire        s_axi_bready,
		output wire [1:0]  s_axi_bresp,
		output wire        s_axi_bvalid,
		output wire        s_axi_arready,
		input  wire [31:0] s_axi_araddr,
		input  wire        s_axi_arvalid,
		input  wire        s_axi_rready,
		output wire [31:0] s_axi_rdata,		
        output wire [1:0]  s_axi_rresp,
        output wire        s_axi_rvalid,
        // *** AXI4-stream master port ***
        input  wire        m_axis_tready,
        output wire [31:0] m_axis_tdata,
        output wire        m_axis_tvalid,
        output wire        m_axis_tlast,
		// *** User port ***
        output wire [1:0]  mod_type,
        output wire [15:0] ifft_config,
        output wire        ifft_config_en,
        output wire [7:0]  guard_interval,
        input wire         done_tick
    );

    // *** Register map ***
    // 0x00: mod_type, guard_interval, and busy
    //       bit 1~0 = mod_type[1:0] (R/W)
    //       bit 9~2 = guard_interval[9:2] (R/W)
    //       bit 10  = busy (R)
    // 0x04: Reserved
    // 0x08: Reserved
    // 0x0C: Reserved
    // 0x10: data register 0
    //       bit 31~0 = data_0[31:0] (R/W)
    // 0x14: data register 1
    //       bit 31~0 = data_1[31:0] (R/W)
    // 0x18: data register 2
    //       bit 31~0 = data_2[31:0] (R/W)
    // 0x1C: data register 3
    //       bit 31~0 = data_3[31:0] (R/W)
    localparam C_ADDR_BITS = 5;
    // *** Memory-mapped Address ***
    localparam C_ADDR_CTRL = 5'h00,
               C_ADDR_DR00 = 5'h10,
               C_ADDR_DR01 = 5'h14,
               C_ADDR_DR02 = 5'h18,
               C_ADDR_DR03 = 5'h1c;            
    // *** AXI write FSM ***
    localparam S_WRIDLE = 2'h0,
               S_WRDATA = 2'h1,
               S_WRRESP = 2'h2;
    // *** AXI read FSM ***
    localparam S_RDIDLE = 2'h0,
               S_RDDATA = 2'h1;
    // *** AXIS FSM ***
    localparam S_IDLE = 2'h0,
               S_WRITE_STREAM = 2'h1;

    // *** AXI write ***
	reg [1:0] wstate_cs, wstate_ns;
	reg [C_ADDR_BITS-1:0] waddr;
	wire [31:0] wmask;
	wire aw_hs, w_hs;
	// *** AXI read ***
	reg [1:0] rstate_cs, rstate_ns;
	wire [C_ADDR_BITS-1:0] raddr;
	reg [31:0] rdata;
	wire ar_hs;
	// *** Internal registers ***
	reg [9:0] ctrl_reg;
    reg [31:0] data_reg [0:3];
    reg start_reg;
    wire [2:0] num_words_w;
    // *** AXIS ***
    reg [1:0] mm2sstate_cs, mm2sstate_ns;
    reg [1:0] wr_ptr_cv, wr_ptr_nv;
    reg tlast_cv, tlast_nv;
    reg busy_reg;

	// *** AXI write ************************************************************
	assign s_axi_awready = (wstate_cs == S_WRIDLE);
	assign s_axi_wready = (wstate_cs == S_WRDATA);
	assign s_axi_bresp = 2'b00;    // OKAY
	assign s_axi_bvalid = (wstate_cs == S_WRRESP);
	assign wmask = {{8{s_axi_wstrb[3]}}, {8{s_axi_wstrb[2]}}, {8{s_axi_wstrb[1]}}, {8{s_axi_wstrb[0]}}};
	assign aw_hs = s_axi_awvalid & s_axi_awready;
	assign w_hs = s_axi_wvalid & s_axi_wready;

	// *** Write state register ***
	always @(posedge aclk)
	begin
		if (!aresetn)
			wstate_cs <= S_WRIDLE;
		else
			wstate_cs <= wstate_ns;
	end
	
	// *** Write state next ***
	always @(*)
	begin
		case (wstate_cs)
			S_WRIDLE:
				if (s_axi_awvalid)
					wstate_ns = S_WRDATA;
				else
					wstate_ns = S_WRIDLE;
			S_WRDATA:
				if (s_axi_wvalid)
					wstate_ns = S_WRRESP;
				else
					wstate_ns = S_WRDATA;
			S_WRRESP:
				if (s_axi_bready)
					wstate_ns = S_WRIDLE;
				else
					wstate_ns = S_WRRESP;
			default:
				wstate_ns = S_WRIDLE;
		endcase
	end
	
	// *** Write address register ***
	always @(posedge aclk)
	begin
		if (aw_hs)
			waddr <= s_axi_awaddr[C_ADDR_BITS-1:0];
	end

	// *** AXI read *************************************************************
	assign s_axi_arready = (rstate_cs == S_RDIDLE);
	assign s_axi_rdata = rdata;
	assign s_axi_rresp = 2'b00;   // OKAY
	assign s_axi_rvalid = (rstate_cs == S_RDDATA);
	assign ar_hs = s_axi_arvalid & s_axi_arready;
	assign raddr = s_axi_araddr[C_ADDR_BITS-1:0];
	
	// *** Read state register ***
	always @(posedge aclk)
	begin
		if (!aresetn)
			rstate_cs <= S_RDIDLE;
		else
			rstate_cs <= rstate_ns;
	end

	// *** Read state next ***
	always @(*) 
	begin
		case (rstate_cs)
			S_RDIDLE:
				if (s_axi_arvalid)
					rstate_ns = S_RDDATA;
				else
					rstate_ns = S_RDIDLE;
			S_RDDATA:
				if (s_axi_rready)
					rstate_ns = S_RDIDLE;
				else
					rstate_ns = S_RDDATA;
			default:
				rstate_ns = S_RDIDLE;
		endcase
	end
	
	// *** Read data register ***
	always @(posedge aclk)
	begin
	    if (!aresetn)
	        rdata <= 0;
		else if (ar_hs)
            case (raddr)
				C_ADDR_CTRL: 
					rdata <= {busy_reg, ctrl_reg[9:0]};
			    C_ADDR_DR00:
                    rdata <= data_reg[0];
			    C_ADDR_DR01:
                    rdata <= data_reg[1];
			    C_ADDR_DR02:
                    rdata <= data_reg[2];
			    C_ADDR_DR03:
                    rdata <= data_reg[3];         			
			endcase
	end

    // *** Internal registers ***************************************************
   	assign mod_type = ctrl_reg[1:0];
   	assign num_words_w = (ctrl_reg[1:0] == 0) ? 1 : ((ctrl_reg[1:0] == 1) ? 2 : 4);
   	assign guard_interval = ctrl_reg[9:2];
   	
   	// *** ctrl_reg[9:0] ***
	always @(posedge aclk)
	begin
	    if (!aresetn)
	    begin
	        ctrl_reg[9:0] <= 0;
	    end
		else if (w_hs && (waddr == C_ADDR_CTRL))
		begin
            ctrl_reg[9:0] <= (s_axi_wdata[31:0] & wmask) | (ctrl_reg[9:0] & ~wmask);
		end
	end

	// *** data_reg[0][31:0] - data_reg[3][31:0] ***
	always @(posedge aclk)
	begin
	    if (!aresetn)
	    begin
	        start_reg <= 0;
	        busy_reg <= 0;
            data_reg[0] <= 0;
            data_reg[1] <= 0;
            data_reg[2] <= 0;
            data_reg[3] <= 0;
        end
		else if (w_hs && waddr == C_ADDR_DR00)
		begin
		    if (num_words_w == 1)
		    begin
                start_reg <= 1;
                busy_reg <= 1;
            end
			data_reg[0][31:0] <= (s_axi_wdata[31:0] & wmask) | (data_reg[0][31:0] & ~wmask);
	    end
		else if (w_hs && waddr == C_ADDR_DR01)
        begin
            if (num_words_w == 2)
            begin
                start_reg <= 1;
                busy_reg <= 1;
            end
            data_reg[1][31:0] <= (s_axi_wdata[31:0] & wmask) | (data_reg[1][31:0] & ~wmask);
        end
		else if (w_hs && waddr == C_ADDR_DR02)
        begin
            data_reg[2][31:0] <= (s_axi_wdata[31:0] & wmask) | (data_reg[2][31:0] & ~wmask);
        end
		else if (w_hs && waddr == C_ADDR_DR03)
        begin
            if (num_words_w == 4)
            begin
                start_reg <= 1;
                busy_reg <= 1;
            end
            data_reg[3][31:0] <= (s_axi_wdata[31:0] & wmask) | (data_reg[3][31:0] & ~wmask);
        end
        else if (done_tick)
        begin
            busy_reg <= 0;
        end
	    else
        begin
            start_reg <= 0;
        end
	end

    // *** AXIS *****************************************************************
    assign m_axis_tdata = data_reg[wr_ptr_cv];
    assign m_axis_tvalid = (mm2sstate_cs == S_WRITE_STREAM) ? 1 : 0;
    assign m_axis_tlast = tlast_cv;
    
    // *** AXIS state register ***
    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            mm2sstate_cs <= S_IDLE;
            wr_ptr_cv <= 0;
            tlast_cv <= 0;
        end
        else
        begin
            mm2sstate_cs <= mm2sstate_ns;
            wr_ptr_cv <= wr_ptr_nv;
            tlast_cv <= tlast_nv;
        end 
    end
    
    // *** AXIS state next ***
    always @(*)
    begin
        mm2sstate_ns = mm2sstate_cs;
        wr_ptr_nv = wr_ptr_cv;
        tlast_nv = tlast_cv;
        case (mm2sstate_cs)
            S_IDLE:
            begin
                if (start_reg)
                begin
                    mm2sstate_ns = S_WRITE_STREAM;
                    if (num_words_w == 1)
                        tlast_nv = 1;    
                end
            end
            S_WRITE_STREAM:
            begin
                if (m_axis_tready)
                begin
                    if (wr_ptr_cv == num_words_w-1)
                    begin
                        mm2sstate_ns = S_IDLE;
                        wr_ptr_nv = 0;
                        tlast_nv = 0;
                    end
                    else
                    begin
                        if (wr_ptr_cv == num_words_w-2)
                        begin
                            tlast_nv = 1;
                        end
                        wr_ptr_nv = wr_ptr_cv + 1;
                    end
                end
            end
        endcase
    end

    // *** IFFT configuration ***************************************************
    // Bit 5~0  = cp_len[5:0]     = 8
    // Bit 8    = fwd_inv         = 0 (inverse)
    // Bit 14~9 = scale_sch[14:9] = 0b101010 (default scaling for radix-4), see Xilinx's datasheet
    // 0b0101_0100_0000_1000 = 0x5408
    assign ifft_config = 16'h5408;
    assign ifft_config_en = 1;

endmodule
