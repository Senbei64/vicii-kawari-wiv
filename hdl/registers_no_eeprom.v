// Init sequence code for board with no EEPROM.

// We only care about the is_reset case.
// NOTE: chip must be set before reset is lifted, otherwise we
// will leave some state machines configured to the wrong
// chip. Make sure chip set point happens before reset lift
// point.
task handle_persist(input is_reset);
    if (is_reset)
    begin
        if (internal_rst)
            rstcntr <= rstcntr + `RESET_CTR_INC;
        if (rstcntr == `RESET_CHIP_SET_POINT)
            chip <= {chip[1], standard_sw ? chip[0] : ~chip[0]};
        if (rstcntr == `RESET_LIFT_POINT)
            rst <= 0;
    end
endtask
