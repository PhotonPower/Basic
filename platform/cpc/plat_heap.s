;! module    plat_heap
;! provides  plat_heap plat_heap_end plat_arr plat_arr_end
;! requires  prg_end
;! group     core
;! clobbers  -
;! doc       cpc: the memory after the program up to the stack: arrays the lower half, strings the upper.

plat_arr        EQU prg_end
plat_heap_end   EQU &A200
plat_heap       EQU (prg_end+plat_heap_end)/2
plat_arr_end    EQU plat_heap
