;! module    rt_error_stub
;! provides  rt_error
;! group     core
;! clobbers  -
;! doc       Conformance tests: records the first error number (A) at &F000 and returns (in a program rt_error does not return).

rt_error:
        PUSH    AF
        LD      A,(&F000)
        CP      &FF
        JR      NZ,rt_error_stub__k
        POP     AF
        LD      (&F000),A
        RET
rt_error_stub__k:
        POP     AF
        RET
