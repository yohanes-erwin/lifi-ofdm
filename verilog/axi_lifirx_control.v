// Author: Erwin Ouyang
// Date  : 23 Feb 2018
// Update: 11 Mar 2018 - Add configurable peak threshold control register
//	       28 Jan 2019 - Peak threshold control removed, GitHub first commit, from lifi_ap_functional_daa

`timescale 1ns / 1ps

module axi_lifirx_control
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
        // *** AXI4-stream slave port ***
        output wire        s_axis_tready,
        input  wire [31:0] s_axis_tdata,
        input  wire        s_axis_tvalid,
        input  wire        s_axis_tlast,
		// *** User port ***
        output wire [1:0]  demod_type,
        output wire [7:0]  fft_config,
        output wire        fft_config_en
    );
    
    // *** Register map ***
    // 0x00: mod_type and done
    //       bit 1~0 = mod_type[1:0] (R/W)
    //       bit 2   = done (R)
    // 0x04: Reserved
    // 0x08: Reserved
    // 0x0C: Reserved
    // 0x10: data register 0
    //       bit 31~0 = data_0[31:0] (R)
    // 0x14: data register 1
    //       bit 31~0 = data_1[31:0] (R)
    // 0x18: data register 2
    //       bit 31~0 = data_2[31:0] (R)
    // 0x1C: data register 3
    //       bit 31~0 = data_3[31:0] (R)
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
               S_READ_STREAM = 2'h1,
               S_BUSY = 2'h2;

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
	reg [1:0] ctrl_reg;
    reg [31:0] data_reg [0:3];
    // *** AXIS ***
    reg [1:0] s2mmstate_cs, s2mmstate_ns;
    reg [1:0] rd_ptr_cv, rd_ptr_nv;
    reg ready_cv, ready_nv;
    reg data_rd_reg;
    
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
	        data_rd_reg <= 0;
	    end
		else if (ar_hs)
		begin
            case (raddr)
				C_ADDR_CTRL:
				begin
					rdata <= {ready_cv, ctrl_reg[1:0]};
			    end
			    C_ADDR_DR00:
			    begin
                    rdata <= data_reg[0];
                    if (ctrl_reg[1:0] == 0)
                        data_rd_reg <= 1;
                end
			    C_ADDR_DR01:
			    begin
                    rdata <= data_reg[1];
                    if (ctrl_reg[1:0] == 1)
                        data_rd_reg <= 1;
                end
			    C_ADDR_DR02:
			    begin
                    rdata <= data_reg[2];
                end
			    C_ADDR_DR03:
			    begin
                    rdata <= data_reg[3];
                    if (ctrl_reg[1:0] == 2)
                        data_rd_reg <= 1;
                end        			
			endcase
        end
        else
        begin
            data_rd_reg <= 0;
        end
	end

    // *** Internal registers ***************************************************
    assign demod_type = ctrl_reg[1:0];
    
   	// *** ctrl_reg[1:0] ***
	always @(posedge aclk)
	begin
	    if (!aresetn)
            ctrl_reg[1:0] <= 0;
		else if (w_hs && waddr == C_ADDR_CTRL)
			ctrl_reg[1:0] <= (s_axi_wdata[31:0] & wmask) | (ctrl_reg[1:0] & ~wmask);
	end

    // *** data_reg[0][31:0] - data_reg[3][31:0] ***
	always @(posedge aclk)
	begin
	    if (!aresetn)
	    begin
            data_reg[0] <= 0;
            data_reg[1] <= 0;
            data_reg[2] <= 0;
            data_reg[3] <= 0;
        end
		else if (s_axis_tvalid && s_axis_tready)
		begin
            data_reg[rd_ptr_cv] <= s_axis_tdata;
        end
	end

    // *** AXIS *****************************************************************
    assign s_axis_tready = (s2mmstate_cs == S_BUSY) ? 0 : 1;

    // *** AXIS state register ***
    always @(posedge aclk)
    begin
        if (!aresetn)
        begin
            s2mmstate_cs <= S_IDLE;
            rd_ptr_cv <= 0;
            ready_cv <= 0;
        end
        else
        begin
            s2mmstate_cs <= s2mmstate_ns;
            rd_ptr_cv <= rd_ptr_nv;
            ready_cv <= ready_nv;
        end 
    end
    
    // *** AXIS state next ***
    always @(*)
    begin
        s2mmstate_ns = s2mmstate_cs;
        rd_ptr_nv = rd_ptr_cv;
        ready_nv = ready_cv;
        case (s2mmstate_cs)
            S_IDLE:
            begin
                if (s_axis_tvalid)
                begin
                    if (s_axis_tlast)
                    begin
                        s2mmstate_ns = S_BUSY;
                        rd_ptr_nv = 0;
                        ready_nv = 1;
                    end
                    else
                    begin
                        s2mmstate_ns = S_READ_STREAM;
                        rd_ptr_nv = rd_ptr_cv + 1;
                    end
                end
            end
            S_READ_STREAM:
            begin
                if (s_axis_tvalid)
                begin
                    if (s_axis_tlast)
                    begin
                        s2mmstate_ns = S_BUSY;
                        rd_ptr_nv = 0;
                        ready_nv = 1;
                    end
                    else
                    begin
                        rd_ptr_nv = rd_ptr_cv + 1;
                    end
                end
            end
            S_BUSY:
            begin
                if (data_rd_reg)
                begin
                    s2mmstate_ns = S_IDLE;
                    ready_nv = 0;
                end
            end
        endcase
    end

    // *** FFT configuration ****************************************************
    // Bit 0 = fwd_inv = 1 (forward)
    assign fft_config = 8'h1;
    assign fft_config_en = 1;
    
endmodule
