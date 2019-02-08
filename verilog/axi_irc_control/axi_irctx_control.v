// Author: Erwin Ouyang
// Date  : 11 Mar 2018
// Update: 26 Sep 2018 - Porting to RedPitaya
//         07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module axi_irctx_control
	#(
        C_ADDR_WIDTH = 32,
        C_DATA_WIDTH = 32
    )
	(
        // *** Clock and reset signals ***
        input  wire                      aclk,
        input  wire                      aresetn,
        // *** AXI4-lite slave signals ***
        output wire                      s_axi_awready,
        input  wire [C_ADDR_WIDTH-1:0]   s_axi_awaddr,
        input  wire                      s_axi_awvalid,
        output wire                      s_axi_wready,
        input  wire [C_DATA_WIDTH-1:0]   s_axi_wdata,
        input  wire [C_DATA_WIDTH/8-1:0] s_axi_wstrb,
        input  wire                      s_axi_wvalid,
        input  wire                      s_axi_bready,
        output wire [1:0]                s_axi_bresp,
        output wire                      s_axi_bvalid,
        output wire                      s_axi_arready,
        input  wire [C_ADDR_WIDTH-1:0]   s_axi_araddr,
        input  wire                      s_axi_arvalid,    
        input  wire                      s_axi_rready,
        output wire [C_DATA_WIDTH-1:0]   s_axi_rdata,
        output wire [1:0]                s_axi_rresp,
        output wire                      s_axi_rvalid,
        // *** AXI4-stream master signals ***
        input  wire                      m_axis_tready,
        output wire [7:0]                m_axis_tdata,
        output wire                      m_axis_tvalid,
        // *** User signals ***
        output wire [15:0]               mod_m,
        output wire                      mod_38khz_en
    );

    // *** Register map ***
    // 0x00: baud rate divisor and tx busy
    //       bit 15~0 = baud[15:0] (R/W)
    //       bit 16   = mod_38khz_en (R/W)
    //       bit 17   = busy_reg (R)
    // 0x04: uart tx data register
    //       bit 7~0 = txdr_reg[7:0] (R/W)
    // 0x08: reserved
    // 0x0c: reserved
	localparam C_ADDR_BITS = 4;
	// *** Address ***
	localparam C_ADDR_CTRL = 4'h0,
			   C_ADDR_TXDR = 4'h4;
	// *** AXI write FSM ***
	localparam S_WRIDLE = 2'd0,
			   S_WRDATA = 2'd1,
			   S_WRRESP = 2'd2;
	// *** AXI read FSM ***
	localparam S_RDIDLE = 2'd0,
			   S_RDDATA = 2'd1;
    // *** AXIS mm2s FSM ***
    localparam S_MM2S_IDLE = 2'h0,
               S_MM2S_WR = 2'h1;

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
	reg [16:0] ctrl_reg;
    reg [7:0] txdr_reg;
    wire busy_com;
    // *** AXIS master ***
    reg [1:0] mm2sstate_cs, mm2sstate_ns;
    reg start_reg;

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
	    begin
	        rdata <= 0;
	    end
		else if (ar_hs)
		begin
            case (raddr)
				C_ADDR_CTRL:
				begin 
					rdata <= {busy_com, ctrl_reg};
			    end
			    C_ADDR_TXDR:
                begin
                    rdata <= txdr_reg;
			    end		
			endcase
	    end
	end

    // *** Internal registers ***************************************************
    assign mod_m = ctrl_reg[15:0];
    assign mod_38khz_en = ctrl_reg[16];

   	// *** ctrl_reg ***
	always @(posedge aclk)
	begin
	    if (!aresetn)
            ctrl_reg[16:0] <= 0;
		else if (w_hs && waddr == C_ADDR_CTRL)
			ctrl_reg[16:0] <= (s_axi_wdata[31:0] & wmask) | (ctrl_reg[16:0] & ~wmask);
	end

    // *** txdr_reg ***
	always @(posedge aclk)
	begin
	    if (!aresetn)
	    begin
            txdr_reg[7:0] <= 0;
            start_reg <= 0;
        end
		else if (w_hs && waddr == C_ADDR_TXDR)
		begin
            txdr_reg[7:0] <= (s_axi_wdata[31:0] & wmask) | (txdr_reg[7:0] & ~wmask);
            start_reg <= 1;
        end
        else
        begin
            start_reg <= 0;
        end
	end

    // *** AXIS master **********************************************************
    assign busy_com = (mm2sstate_cs == S_MM2S_WR) ? 1 : 0;
    assign m_axis_tdata = txdr_reg;
    assign m_axis_tvalid = (mm2sstate_cs == S_MM2S_WR) ? 1 : 0;
    
    // *** AXIS master state register ***
    always @(posedge aclk)
    begin
        if (!aresetn)
            mm2sstate_cs <= S_MM2S_IDLE;
        else
            mm2sstate_cs <= mm2sstate_ns;
    end
    
    // *** AXIS master state next ***
    always @(*)
    begin
        mm2sstate_ns = mm2sstate_cs;
        case (mm2sstate_cs)
            S_MM2S_IDLE:
            begin
                if (start_reg)
                    mm2sstate_ns = S_MM2S_WR;
            end
            S_MM2S_WR:
            begin
                if (m_axis_tready)
                begin
                    mm2sstate_ns = S_MM2S_IDLE;
                end
            end
        endcase
    end
                           
endmodule
