;! module    plat_arr
;! provides  plat_arr plat_arr_end
;! group     core
;! clobbers  -
;! doc       z80-test: the array area &A000..&BFFF (below the string heap).

plat_arr        EQU &A000
plat_arr_end    EQU &C000
