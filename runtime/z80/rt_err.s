;! module    rt_err
;! provides  rt_error
;! requires  plat_error
;! group     core
;! clobbers  AF BC DE HL
;! doc       Runtime errors (A = number): without ON ERROR GOSUB to the platform (plat_error).

rt_error:
        JP      plat_error
