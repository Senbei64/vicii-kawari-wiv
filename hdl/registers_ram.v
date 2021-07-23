
`ifdef HAVE_EEPROM
task persist_eeprom(input do_persist, input [7:0] reg_num, input [7:0] reg_val);
    // This version writes to the eeprom
    if (do_persist) begin
        // persistence_lock must be CLOSED to allow
        // chip model is exempt
        if (~persistence_lock || reg_num == `EXT_REG_CHIP_MODEL) begin
           eeprom_busy <= 1'b1;
           // Registers < PER_CHIP_REG_START are cross-chip settings.
           // For any register above PER_CHIP_REG_START, use eeprom_bank to
           // select 1 of 4 256 byte banks to save into.
           if (reg_num > `PER_CHIP_REG_START)
               eeprom_w_addr <= {eeprom_bank, reg_num};
           else
               eeprom_w_addr <= {2'b0, reg_num};
           eeprom_w_value <= reg_val;
           state_ctr_reset_for_write <= 1'b1;
        end
    end
endtask
`else
task persist_eeprom(input do_persist, input [7:0] reg_num, input [7:0] reg_val);
    ; // noop
endtask
`endif

// For color ram:
//     flip read bit on and set address and which 6-bit-nibble (out of 4)
//     is to be read, dbo will be set by the 'CPU read from color regs' block
//     above.
// For video ram:
//     flip read bit on and set address. dbo will be set by the
//     'CPU read from video ram' block above.
//
// In both cases, read happens next cycle and r flags turned off.
//
// If overlay is on, ram_hi and ram_idx are ignored and it will never
// trigger a vram read.
task read_ram(
        input overlay,
        input [7:0] ram_lo,
        input [7:0] ram_hi,
        input [7:0] ram_idx);
    begin
        if (overlay) begin
            if (ram_lo >= 8'h40 && ram_lo < 8'h80) begin
                // _r_nibble stores which 6-bit-nibble within the 24 bit
                // lookup value we want.  The lowest 6-bits are never used.
`ifdef NEED_RGB
`ifdef CONFIGURABLE_RGB
                color_regs_r <= 1'b1;
                color_regs_r_nibble <= ram_lo[1:0];
                color_regs_addr_a <= ram_lo[5:2];
`endif
`endif
            end
`ifdef CONFIGURABLE_LUMAS
            /* verilator lint_off WIDTH */
            else if (ram_lo >= `EXT_REG_LUMA0 && ram_lo <= `EXT_REG_LUMA15) begin
                luma_regs_r <= 1'b1;
                luma_regs_r_nibble <= 2'b00; // luma
                luma_regs_addr_a <= ram_lo[3:0];
            end
            else if (ram_lo >= `EXT_REG_PHASE0 && ram_lo <= `EXT_REG_PHASE15) begin
                luma_regs_r <= 1'b1;
                luma_regs_r_nibble <= 2'b01; // phase
                luma_regs_addr_a <= ram_lo[3:0];
            end
            else if (ram_lo >= `EXT_REG_AMPL0 && ram_lo <= `EXT_REG_AMPL15) begin
                luma_regs_r <= 1'b1;
                luma_regs_r_nibble <= 2'b10; // amplitude
                luma_regs_addr_a <= ram_lo[3:0];
            end
            /* verilator lint_on WIDTH */
`endif
            else begin
                case (ram_lo)
                    `EXT_REG_MAGIC_0:
                        dbo <= magic_1;
                    `EXT_REG_MAGIC_1:
                        dbo <= magic_2;
                    `EXT_REG_MAGIC_2:
                        dbo <= magic_3;
                    `EXT_REG_MAGIC_3:
                        dbo <= magic_4;
                    `EXT_REG_CHIP_MODEL:
                        dbo <= {6'b0, chip};
`ifdef HAVE_EEPROM
                    `EXT_REG_EEPROM_BANK:
                        dbo <= {6'b0, eeprom_bank};
`endif
                    `EXT_REG_DISPLAY_FLAGS:
`ifdef NEED_RGB
                        dbo <= {1'b0, // reserved
                                ~standard_sw,
                                last_hpolarity,
                                last_vpolarity,
                                last_enable_csync,
                                last_is_native_x,
                                last_is_native_y,
                                last_raster_lines};
`else
                        dbo <= {1'b0, // reserved
                               ~standard_sw,
                               6'b0};
`endif
`ifdef HIRES_MODES
                    `EXT_REG_CURSOR_LO:
                        dbo <= hires_cursor_lo;
                    `EXT_REG_CURSOR_HI:
                        dbo <= hires_cursor_hi;
`endif
                    `EXT_REG_VERSION:
                        dbo <= {`VERSION_MAJOR, `VERSION_MINOR};
                    `EXT_REG_VARIANT_NAME1:
                        dbo <= `VARIANT_NAME1;
                    `EXT_REG_VARIANT_NAME2:
                        dbo <= `VARIANT_NAME2;
                    `EXT_REG_VARIANT_NAME3:
                        dbo <= `VARIANT_NAME3;
                    `EXT_REG_VARIANT_NAME4:
                        dbo <= `VARIANT_NAME4;
                    `EXT_REG_VARIANT_NAME5:
                        dbo <= 8'd0;
`ifdef CONFIGURABLE_LUMAS
                    `EXT_REG_BLANKING:
                        dbo <= {2'b0, blanking_level};
                    `EXT_REG_BURSTAMP:
                        dbo <= {4'b0, burst_amplitude};
`endif
                    // Advertise some capability bits
                    `EXT_REG_CAP_LO:
                    begin
                        dbo[`CAP_RGB_BIT] <= `HAS_RGB_CAP;
                        dbo[`CAP_DVI_BIT] <= `HAS_DVI_CAP;
                        dbo[`CAP_COMP_BIT] <= `HAS_COMP_CAP;
                        dbo[`CAP_CONFIG_RGB_BIT] <= `HAS_CONFIG_RGB_CAP;
                        dbo[`CAP_CONFIG_LUMA_BIT] <= `HAS_CONFIG_LUMA_CAP;
                        dbo[`CAP_CONFIG_TIMING_BIT] <= `HAS_CONFIG_TIMING_CAP;
                        dbo[`CAP_PERSIST_BIT] <= `HAS_PERSIST_CAP;
                        dbo[`CAP_HIRES_BIT] <= `HAS_HIRES_CAP;
                    end
                    `EXT_REG_CAP_HI:
                        dbo <= 8'b0; // reserved for now
`ifdef CONFIGURABLE_TIMING
                    `EXT_REG_TIMING_CHANGE:
                        dbo <= {7'b0, timing_change};
                    8'hd0:
                        dbo <= timing_h_blank_ntsc;
                    8'hd1:
                        dbo <= timing_h_fporch_ntsc;
                    8'hd2:
                        dbo <= timing_h_sync_ntsc;
                    8'hd3:
                        dbo <= timing_h_bporch_ntsc;
                    8'hd4:
                        dbo <= timing_v_blank_ntsc;
                    8'hd5:
                        dbo <= timing_v_fporch_ntsc;
                    8'hd6:
                        dbo <= timing_v_sync_ntsc;
                    8'hd7:
                        dbo <= timing_v_bporch_ntsc;
                    8'hd8:
                        dbo <= timing_h_blank_pal;
                    8'hd9:
                        dbo <= timing_h_fporch_pal;
                    8'hda:
                        dbo <= timing_h_sync_pal;
                    8'hdb:
                        dbo <= timing_h_bporch_pal;
                    8'hdc:
                        dbo <= timing_v_blank_pal;
                    8'hdd:
                        dbo <= timing_v_fporch_pal;
                    8'hde:
                        dbo <= timing_v_sync_pal;
                    8'hdf:
                        dbo <= timing_v_bporch_pal;
`endif
                    default: ;
                endcase
            end
        end else begin
            video_ram_r <= 1;
            video_ram_addr_a <= {ram_hi[6:0], ram_lo} + {7'b0, ram_idx};
        end
    end
endtask

// For color ram:
//     Write happens in two stages. First pre_wr flag is set along with
//     value and which 6-bit-nibble (of 4) and the adddress.  When stage 1 is
//     handled above, the value is read out first, the nibble updated
//     and then the write op is done.
// For video ram:
//     Write happens in one stage. We set the wr flag, address and value
//     here.
//
// In both cases, wr flags are turned one cycle after they are set.
//
// If overlay is on, ram_hi and ram_idx are ignored and it will never
// trigger a vram write.
//
// If do_persist is 1, we save the change to eeprom.
task write_ram(
        input overlay,
        input [7:0] ram_lo,
        input [7:0] ram_hi,
        input [7:0] ram_idx,
        input [7:0] data,
        input from_cpu,
        input do_persist);
    begin
        if (overlay) begin
            if (ram_lo >= 8'h40 && ram_lo < 8'h80) begin
                // In order to write to individual 6 bit
                // values within the 24 bit register, we
                // have to read it first, then write.
`ifdef NEED_RGB
`ifdef CONFIGURABLE_RGB
`ifdef HAVE_EEPROM
                if (eeprom_bank == chip) begin
`endif
                   color_regs_pre_wr2_a <= 1'b1;
                   color_regs_wr_value <= data[5:0];
                   color_regs_wr_nibble <= ram_lo[1:0];
                   color_regs_addr_a <= ram_lo[5:2];
`ifdef HAVE_EEPROM
                end
`endif
                persist_eeprom(do_persist, ram_lo, {2'b0, data[5:0]});
`endif
`endif
            end
`ifdef CONFIGURABLE_LUMAS
            /* verilator lint_off WIDTH */
            else if (ram_lo >= `EXT_REG_LUMA0 && ram_lo <= `EXT_REG_LUMA15) begin
`ifdef HAVE_EEPROM
                if (eeprom_bank == chip) begin
`endif
                   luma_regs_pre_wr2_a <= 1'b1;
                   luma_regs_wr_value <= {2'b0, data[5:0]};
                   luma_regs_wr_nibble <= 2'b00; // luma
                   luma_regs_addr_a <= ram_lo[3:0];
`ifdef HAVE_EEPROM
                end
`endif
                persist_eeprom(do_persist, ram_lo, {2'b0, data[5:0]});
            end
            else if (ram_lo >= `EXT_REG_PHASE0 && ram_lo <= `EXT_REG_PHASE15) begin
`ifdef HAVE_EEPROM
                if (eeprom_bank == chip) begin
`endif
                   luma_regs_pre_wr2_a <= 1'b1;
                   luma_regs_wr_value <= data[7:0];
                   luma_regs_wr_nibble <= 2'b01; // phase
                   luma_regs_addr_a <= ram_lo[3:0];
`ifdef HAVE_EEPROM
                end
`endif
                persist_eeprom(do_persist, ram_lo, data[7:0]);
            end
            else if (ram_lo >= `EXT_REG_AMPL0 && ram_lo <= `EXT_REG_AMPL15) begin
`ifdef HAVE_EEPROM
                if (eeprom_bank == chip) begin
`endif
                   luma_regs_pre_wr2_a <= 1'b1;
                   luma_regs_wr_value <= {4'b0, data[3:0]};
                   luma_regs_wr_nibble <= 2'b10; // amplitude
                   luma_regs_addr_a <= ram_lo[3:0];
`ifdef HAVE_EEPROM
                end
`endif
                persist_eeprom(do_persist, ram_lo, {4'b0, data[3:0]});
            end
            /* verilator lint_on WIDTH */
`endif
            else begin
                // When we poke certain config registers, we
                // persist them according to persist_eeprom
                // implementation.
                case (ram_lo)
                    `EXT_REG_MAGIC_0: begin
                        magic_1 <= data;
                        persist_eeprom(do_persist, `EXT_REG_MAGIC_0, data);
                    end
                    `EXT_REG_MAGIC_1: begin
                        magic_2 <= data;
                        persist_eeprom(do_persist, `EXT_REG_MAGIC_1, data);
                    end
                    `EXT_REG_MAGIC_2: begin
                        magic_3 <= data;
                        persist_eeprom(do_persist, `EXT_REG_MAGIC_2, data);
                    end
                    `EXT_REG_MAGIC_3: begin
                        magic_4 <= data;
                        persist_eeprom(do_persist, `EXT_REG_MAGIC_3, data);
                    end
                    // Not safe to allow nativex/y to be changed from
                    // CPU. Already burned by this with accidental
                    // overwrite of this register. This can effectively
                    // disable your display so leave this only to the
                    // serial connection to change.
                    `EXT_REG_CHIP_MODEL:
                    begin
                        // We never change chip register from CPU. Only
                        // init sequence will set it after restoring it from
                        // EEPROM, or if no EEPROM, it will be determined
                        // by the default value + possibly h/w switch.
                        persist_eeprom(do_persist, `EXT_REG_CHIP_MODEL, {6'b0, data[1:0]});
                    end
`ifdef HAVE_EEPROM
                    `EXT_REG_EEPROM_BANK:
                    begin
                        if (from_cpu)
                            eeprom_bank <= data[1:0];
                    end
`endif
                    `EXT_REG_DISPLAY_FLAGS:
                    begin
`ifdef NEED_RGB
                        last_raster_lines <= data[`SHOW_RASTER_LINES_BIT];
                        last_is_native_y <= data[`IS_NATIVE_Y_BIT]; // 15khz
                        last_is_native_x <= data[`IS_NATIVE_X_BIT];
                        last_enable_csync <= data[`ENABLE_CSYNC_BIT];
                        last_hpolarity <= data[`HPOLARITY_BIT];
                        last_vpolarity <= data[`VPOLARITY_BIT];
                        persist_eeprom(do_persist, `EXT_REG_DISPLAY_FLAGS,
                                       {
                                        2'b0,
                                        data[`VPOLARITY_BIT],
                                        data[`HPOLARITY_BIT],
                                        data[`ENABLE_CSYNC_BIT],
                                        data[`IS_NATIVE_X_BIT],
                                        data[`IS_NATIVE_Y_BIT],
                                        data[`SHOW_RASTER_LINES_BIT]
                                       });
`endif // NEED_RGB
                    end
`ifdef HIRES_MODES
                    `EXT_REG_CURSOR_LO:
                        hires_cursor_lo <= data;
                    `EXT_REG_CURSOR_HI:
                        hires_cursor_hi <= data;
`endif
`ifdef CONFIGURABLE_LUMAS
                    `EXT_REG_BLANKING: begin
`ifdef HAVE_EEPROM
                if (eeprom_bank == chip) begin
`endif
                        blanking_level <= data[5:0];
`ifdef HAVE_EEPROM
                end
`endif
                        persist_eeprom(do_persist, ram_lo, {2'b0, data[5:0]});
                    end
                    `EXT_REG_BURSTAMP: begin
`ifdef HAVE_EEPROM
                if (eeprom_bank == chip) begin
`endif
                        burst_amplitude <= data[3:0];
`ifdef HAVE_EEPROM
                end
`endif
                        persist_eeprom(do_persist, ram_lo, {4'b0, data[3:0]});
                    end
`endif
`ifdef CONFIGURABLE_TIMING
                    `EXT_REG_TIMING_CHANGE:
                        timing_change <= data[0];

                    8'hd0:
                        timing_h_blank_ntsc <= data;
                    8'hd1:
                        timing_h_fporch_ntsc <= data;
                    8'hd2:
                        timing_h_sync_ntsc <= data;
                    8'hd3:
                        timing_h_bporch_ntsc <= data;
                    8'hd4:
                        timing_v_blank_ntsc <= data;
                    8'hd5:
                        timing_v_fporch_ntsc <= data;
                    8'hd6:
                        timing_v_sync_ntsc <= data;
                    8'hd7:
                        timing_v_bporch_ntsc <= data;
                    8'hd8:
                        timing_h_blank_pal <= data;
                    8'hd9:
                        timing_h_fporch_pal <= data;
                    8'hda:
                        timing_h_sync_pal <= data;
                    8'hdb:
                        timing_h_bporch_pal <= data;
                    8'hdc:
                        timing_v_blank_pal <= data;
                    8'hdd:
                        timing_v_fporch_pal <= data;
                    8'hde:
                        timing_v_sync_pal <= data;
                    8'hdf:
                        timing_v_bporch_pal <= data;
`endif
                    default: ;
                endcase
            end
        end else begin
            video_ram_wr_a <= 1'b1;
            video_ram_aw <= 1'b1;
            video_ram_data_in_a <= data[7:0];
            video_ram_addr_a <= {ram_hi[6:0], ram_lo} + {7'b0, ram_idx};
        end
    end
endtask

