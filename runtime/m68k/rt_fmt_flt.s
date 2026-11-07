;! module    rt_fmt_flt
;! provides  rt_fmt_flt
;! requires  rt_flt_dec
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Float -> text as PRINT/STR$ (13 significant digits or DEFNUM), rules from the original.

; In:  D0:D1 = x, D2.W = digits (values below 2 count as 2, above 13 as 13), A0 = buffer (32 bytes)
; Out: D0.L = length; the text in the buffer, 0-terminated.
; Without exponent if -5 <= d <= 13, else d.ddd E+dd (runtime/ref/rt_fmt_flt.c).

rt_fmt_flt:
        LINK    A6,#-16                 ; digits buffer
        MOVEM.L D2-D7/A2-A3,-(SP)
        MOVE.L  A0,A2                   ; A2 = start of the output
        MOVE.L  A0,A3                   ; A3 = write pointer
        CMP.W   #2,D2
        BGE     .lo
        MOVEQ   #2,D2
.lo:    CMP.W   #13,D2
        BLE     .hi
        MOVEQ   #13,D2
.hi:    MOVE.W  D2,D7                   ; D7 = digits
        EXT.L   D7
        MOVE.L  D0,D3
        AND.L   #$7FF00000,D3
        BNE     .nz
        MOVE.B  #'0',(A3)+
        BRA     .end
.nz:    LEA     -16(A6),A0
        JSR     rt_flt_dec
        MOVE.L  D0,D5                   ; D5 = d
        TST.L   D1
        BEQ     .noneg
        MOVE.B  #'-',(A3)+
.noneg: LEA     -16(A6),A1              ; A1 = digits
        MOVE.L  D7,D6                   ; D6 = len without trailing zeros (at least 1)
.trim:  CMP.L   #1,D6
        BLE     .layout
        CMP.B   #'0',-1(A1,D6.L)
        BNE     .layout
        SUBQ.L  #1,D6
        BRA     .trim
.layout:
        CMP.L   #-5,D5
        BLT     .expo
        CMP.L   #13,D5
        BGT     .expo
        TST.L   D5
        BMI     .frac
        MOVEQ   #0,D4                   ; integer digits: i = 0..d, beyond len '0'
.ip:    CMP.L   D6,D4
        BGE     .ipz
        MOVE.B  0(A1,D4.L),(A3)+
        BRA     .ipn
.ipz:   MOVE.B  #'0',(A3)+
.ipn:   ADDQ.L  #1,D4
        CMP.L   D5,D4
        BLE     .ip
        CMP.L   D6,D4                   ; len > d + 1: '.' and the rest
        BGE     .end
        MOVE.B  #'.',(A3)+
.fp:    MOVE.B  0(A1,D4.L),(A3)+
        ADDQ.L  #1,D4
        CMP.L   D6,D4
        BLT     .fp
        BRA     .end
.frac:  MOVE.B  #'0',(A3)+              ; d < 0: "0." zeros digits
        MOVE.B  #'.',(A3)+
        MOVE.L  D5,D4
        NOT.L   D4                      ; -d - 1 zeros
        BRA     .zc
.zl:    MOVE.B  #'0',(A3)+
        SUBQ.L  #1,D4
.zc:    TST.L   D4
        BGT     .zl
        MOVEQ   #0,D4
.fd:    MOVE.B  0(A1,D4.L),(A3)+
        ADDQ.L  #1,D4
        CMP.L   D6,D4
        BLT     .fd
        BRA     .end
.expo:  MOVE.B  (A1),(A3)+              ; d.ddd E+dd
        MOVE.B  #'.',(A3)+
        CMP.L   #1,D6
        BGT     .em
        MOVE.B  #'0',(A3)+
        BRA     .ee
.em:    MOVEQ   #1,D4
.eml:   MOVE.B  0(A1,D4.L),(A3)+
        ADDQ.L  #1,D4
        CMP.L   D6,D4
        BLT     .eml
.ee:    MOVE.B  #'E',(A3)+
        MOVE.B  #'+',D0
        TST.L   D5
        BPL     .esgn
        MOVE.B  #'-',D0
        NEG.L   D5
.esgn:  MOVE.B  D0,(A3)+
        CMP.L   #100,D5                 ; at least two exponent digits (|d| <= 324)
        BLT     .e2
        DIVU    #100,D5
        ADD.B   #'0',D5
        MOVE.B  D5,(A3)+
        CLR.W   D5
        SWAP    D5
.e2:    DIVU    #10,D5
        ADD.B   #'0',D5
        MOVE.B  D5,(A3)+
        SWAP    D5
        ADD.B   #'0',D5
        MOVE.B  D5,(A3)+
.end:   CLR.B   (A3)
        MOVE.L  A3,D0
        SUB.L   A2,D0
        MOVEM.L (SP)+,D2-D7/A2-A3
        UNLK    A6
        RTS
