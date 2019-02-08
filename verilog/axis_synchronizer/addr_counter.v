// Author: Erwin Ouyang
// Date  : 7 Feb 2019
// Update: 07 Feb 2019 - GitHub first commit, from lifi_ap_v2_sync

`timescale 1ns / 1ps

module addr_counter
    (
        input wire         clk,
        input wire         rst_n,
        input wire         wea,
        output wire [12:0] addra,
        input wire         reb,
        input wire         addrb_load_en,
        input wire [12:0]  addrb_load,
        output wire [12:0] addrb
    );
    
    reg [12:0] addra_reg, addrb_reg;
    
    assign addra = addra_reg;
    assign addrb = addrb_reg;
    
    always @(posedge clk)
    begin
        if (!rst_n)
        begin
            addra_reg <= 0;
            addrb_reg <= 0;
        end
        else
        begin
            if (wea)
                addra_reg <= addra_reg + 1;
            if (addrb_load_en)
                addrb_reg <= addrb_load;
            if (reb)
                addrb_reg <= addrb_reg + 1;
        end
    end
    
endmodule
