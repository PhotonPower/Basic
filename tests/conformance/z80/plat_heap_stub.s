;! module    plat_heap_stub
;! provides  plat_heap plat_heap_end
;! group     core
;! clobbers  -
;! doc       Conformance test: the string heap &C000..&DEFF, below the buffers of the test (&E000..).

plat_heap       EQU &C000
plat_heap_end   EQU &DF00
