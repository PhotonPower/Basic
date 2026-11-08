;! module    rt_error_stub
;! provides  rt_error
;! group     core
;! clobbers  -
;! doc       Conformance tests: records the error number (A) at &F000 and returns.

rt_error:
        LD      (&F000),A
        RET
