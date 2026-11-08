;! module    plat_heap
;! provides  plat_heap plat_heap_end
;! group     core
;! clobbers  -
;! doc       z80-test: the string heap &C000..&EFFF (the program with its variables lies below, the stack above).

plat_heap       EQU &C000
plat_heap_end   EQU &F000
