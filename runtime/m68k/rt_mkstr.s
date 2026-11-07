;! module    rt_mkstr
;! provides  rt_mki rt_mkl rt_mks rt_mkd rt_cvi rt_cvl rt_cvs rt_cvd
;! requires  rt_str_newtemp fp_finish rt_error
;! group     string
;! clobbers  D0-D3/A0-A1
;! doc       MKI$ MKL$ MKS$ MKD$ and CVI CVL CVS CVD (runtime/ref/rt_mk.c): big-endian, GFA 8-byte float, IEEE single.

; MKx$: D0 (integer) or D0:D1 (float) -> A0 = temporary string. CVx: A0 = descriptor -> D0 or D0:D1;
; a shorter string counts as padded with zero bytes.

; D0 = count, rt_mkstr__b = bytes -> A0 = temporary with them
rt_mkstr__temp:
        MOVE.L  D0,-(SP)
        JSR     rt_str_newtemp          ; A1 = descriptor, A0 = characters
        MOVE.L  (SP)+,D0
        LEA     rt_mkstr__b,A0
        MOVE.L  (A1),D1
        EXG     D1,A1                   ; A1 = characters, D1 = descriptor
        BRA     .n
.c:     MOVE.B  (A0)+,(A1)+
.n:     SUBQ.L  #1,D0
        BPL     .c
        MOVE.L  D1,A0
        RTS

; A0 = descriptor, D0 = count -> the first D0 bytes in rt_mkstr__b (missing ones 0)
rt_mkstr__get:
        LEA     rt_mkstr__b,A1
        CLR.L   (A1)
        CLR.L   4(A1)
        MOVEQ   #0,D1
        MOVE.W  4(A0),D1
        CMP.L   D0,D1
        BLS     .n
        MOVE.L  D0,D1
.n:     MOVE.L  (A0),A0
        BRA     .cn
.c:     MOVE.B  (A0)+,(A1)+
.cn:    SUBQ.L  #1,D1
        BPL     .c
        RTS

rt_mki:
        MOVE.W  D0,rt_mkstr__b
        MOVEQ   #2,D0
        BRA     rt_mkstr__temp

rt_mkl:
        MOVE.L  D0,rt_mkstr__b
        MOVEQ   #4,D0
        BRA     rt_mkstr__temp

rt_cvi:
        MOVEQ   #2,D0
        BSR     rt_mkstr__get
        MOVE.W  rt_mkstr__b,D0
        EXT.L   D0
        RTS

rt_cvl:
        MOVEQ   #4,D0
        BSR     rt_mkstr__get
        MOVE.L  rt_mkstr__b,D0
        RTS

; D0:D1 = x -> MKD$(x): 48-bit mantissa (rounded, ties to even), exponent word = IEEE exponent
; (+1 if the rounding carries), negated for negative numbers
rt_mkd:
        MOVEM.L D4-D5,-(SP)
        CLR.L   rt_mkstr__b
        CLR.L   rt_mkstr__b+4
        MOVE.L  D0,D4                   ; D4 = sign and exponent
        MOVE.L  D0,D5
        AND.L   #$7FF00000,D5
        BEQ     .out                    ; 0: eight zero bytes
        SWAP    D5
        LSR.W   #4,D5                   ; D5 = biased exponent
        AND.L   #$000FFFFF,D0
        BSET    #20,D0                  ; D0:D1 = 53-bit mantissa
        MOVE.L  D1,D2
        AND.L   #31,D2                  ; D2 = the 5 bits below the 48
        MOVEQ   #4,D3
.sh:    LSR.L   #1,D0                   ; >> 5
        ROXR.L  #1,D1
        DBRA    D3,.sh
        CMP.L   #16,D2                  ; round to nearest, ties to even
        BHI     .up
        BCS     .done
        BTST    #0,D1
        BEQ     .done
.up:    ADDQ.L  #1,D1
        BCC     .nc
        ADDQ.L  #1,D0
.nc:    BTST    #16,D0                  ; carried into bit 48: shift back, exponent + 1
        BEQ     .done
        LSR.L   #1,D0
        ROXR.L  #1,D1
        ADDQ.W  #1,D5
.done:  TST.L   D4
        BPL     .pos
        NEG.W   D5
.pos:   MOVE.W  D0,rt_mkstr__b          ; mantissa bits 47..32
        MOVE.L  D1,rt_mkstr__b+2        ; bits 31..0
        MOVE.W  D5,rt_mkstr__b+6
.out:   MOVEM.L (SP)+,D4-D5
        MOVEQ   #8,D0
        BRA     rt_mkstr__temp

; A0 = descriptor -> D0:D1 = CVD(a$) (outside binary64: error 1 or 0)
rt_cvd:
        MOVEQ   #8,D0
        BSR     rt_mkstr__get
        MOVEQ   #0,D0
        MOVE.W  rt_mkstr__b,D0          ; D0:D1 = 48-bit mantissa
        MOVE.L  rt_mkstr__b+2,D1
        MOVE.L  D0,D2
        OR.L    D1,D2
        BEQ     .zero
        MOVEQ   #0,D3                   ; sign in bit 31 of D3
        MOVE.W  rt_mkstr__b+6,D2
        EXT.L   D2
        BPL     .p
        NEG.L   D2
        BSET    #31,D3
.p:     SUB.L   #$3FE+48,D2             ; value = m * 2^(w - $3FE - 48)
        JMP     fp_finish
.zero:  MOVEQ   #0,D0
        MOVEQ   #0,D1
        RTS

; D0:D1 = x -> MKS$(x): IEEE single (rounded, ties to even; overflow error 1; tiny 0)
rt_mks:
        MOVEM.L D4-D5,-(SP)
        CLR.L   rt_mkstr__b
        MOVE.L  D0,D4
        MOVE.L  D0,D5
        AND.L   #$7FF00000,D5
        BEQ     .out
        SWAP    D5
        LSR.W   #4,D5
        EXT.L   D5                      ; D5 = biased exponent
        AND.L   #$000FFFFF,D0
        BSET    #20,D0
        MOVE.L  D1,D2                   ; D2 = the 29 bits below the 24
        AND.L   #$1FFFFFFF,D2
        LSL.L   #3,D0                   ; D0 = m >> 29: (high << 3) | (low >> 29)
        ROL.L   #3,D1
        AND.L   #7,D1
        OR.L    D1,D0
        CMP.L   #$10000000,D2
        BHI     .up
        BCS     .round
        BTST    #0,D0
        BEQ     .round
.up:    ADDQ.L  #1,D0
.round: BTST    #24,D0
        BEQ     .e
        LSR.L   #1,D0
        ADDQ.L  #1,D5
.e:     SUB.L   #896,D5                 ; single exponent = double exponent - 1023 + 127
        CMP.L   #255,D5
        BLT     .fit
        MOVEQ   #1,D0                   ; overflow
        JSR     rt_error
        BRA     .out
.fit:   TST.L   D5
        BLE     .out                    ; below the smallest normal single: 0
        AND.L   #$007FFFFF,D0
        SWAP    D5
        LSL.L   #7,D5                   ; exponent to bits 30..23
        OR.L    D5,D0
        AND.L   #$80000000,D4
        OR.L    D4,D0
        MOVE.L  D0,rt_mkstr__b
.out:   MOVEM.L (SP)+,D4-D5
        MOVEQ   #4,D0
        BRA     rt_mkstr__temp

; A0 = descriptor -> D0:D1 = CVS(a$) (infinity, NaN: error 1; denormal: 0)
rt_cvs:
        MOVEQ   #4,D0
        BSR     rt_mkstr__get
        MOVE.L  rt_mkstr__b,D0
        MOVE.L  D0,D2
        SWAP    D2
        LSR.W   #7,D2
        AND.L   #$FF,D2                 ; D2 = exponent
        BEQ     .zero
        CMP.L   #255,D2
        BNE     .ok
        MOVEQ   #1,D0
        JSR     rt_error
.zero:  MOVEQ   #0,D0
        MOVEQ   #0,D1
        RTS
.ok:    MOVE.L  D0,D3                   ; fraction (23 bits): high 20 bits to D0, low 3 bits to D1
        AND.L   #$007FFFFF,D3
        MOVE.L  D3,D1
        ROR.L   #3,D1
        AND.L   #$E0000000,D1
        LSR.L   #3,D3
        ADD.L   #896,D2                 ; double exponent
        SWAP    D2
        LSL.L   #4,D2                   ; to bits 30..20
        AND.L   #$80000000,D0
        OR.L    D2,D0
        OR.L    D3,D0
        RTS

;! section bss
rt_mkstr__b:
        DS.L    2
