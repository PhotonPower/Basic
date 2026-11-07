;! module    rt_error_stub
;! provides  rt_error
;! group     core
;! doc       Conformance tests only: records the error number (D0) at $7F000 and returns.

rt_error:
        MOVE.L  D0,$7F000
        RTS
