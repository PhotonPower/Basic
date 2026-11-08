;! module    rt_z
;! provides  rt_argp
;! group     core
;! clobbers  -
;! doc       The table of the arguments 3, 4, ... of a runtime call (pointers or values, D30).

;! section bss
rt_argp:
        DS      16
