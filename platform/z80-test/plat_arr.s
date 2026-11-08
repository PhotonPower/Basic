;! module    plat_arr
;! provides  plat_arr plat_arr_end
;! requires  prg_end
;! group     core
;! clobbers  -
;! doc       z80-test: the array area from the end of the program up to the string heap (&C000).

plat_arr        EQU prg_end
plat_arr_end    EQU &C000
