;! module    rt_bits
;! provides  rt_btst rt_bclr rt_bset rt_bchg rt_shift rt_swap rt_word rt_card rt_byte
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       Bit functions: BTST/BCLR/BSET/BCHG, SHL/SHR/ROL/ROR in 8, 16, 32 bits, SWAP, WORD, CARD, BYTE.

; Bit operations: D0 = value, D1 = bit number (mod 32, as the CPU does it for registers) -> D0.
rt_btst:
        BTST    D1,D0
        SNE     D0
        EXT.W   D0
        EXT.L   D0
        RTS
rt_bclr:
        BCLR    D1,D0
        RTS
rt_bset:
        BSET    D1,D0
        RTS
rt_bchg:
        BCHG    D1,D0
        RTS

; D0 = value, D1 = count (mod 32), D2 = width (8, 16, 32), D3 = kind (0 SHL, 1 SHR, 2 ROL, 3 ROR)
; -> D0 (16 bits sign-extended, 8 bits unsigned; semantics of platform/pc brt::shift)
rt_shift:
        MOVE.L  D4,-(SP)
        AND.L   #31,D1
        MOVEQ   #-1,D4                  ; D4 = mask
        CMP.W   #32,D2
        BEQ     .m
        MOVEQ   #1,D4
        LSL.L   D2,D4
        SUBQ.L  #1,D4
.m:     AND.L   D4,D0
        CMP.W   #2,D3
        BCC     .rot
        CMP.L   D2,D1                   ; shift: count >= width gives 0
        BCS     .sh
        MOVEQ   #0,D0
        BRA     .done
.sh:    TST.W   D3
        BNE     .shr
        LSL.L   D1,D0
        AND.L   D4,D0
        BRA     .done
.shr:   LSR.L   D1,D0
        BRA     .done
.rot:   DIVU    D2,D1                   ; count mod width (count < 32)
        SWAP    D1
        AND.L   #$FFFF,D1
        BEQ     .done
        CMP.W   #2,D3
        BEQ     .rol
        NEG.L   D1                      ; ROR n = ROL (width - n)
        ADD.L   D2,D1
.rol:   MOVE.L  D0,D3                   ; (v << n) | (v >> (width - n))
        LSL.L   D1,D0
        NEG.L   D1
        ADD.L   D2,D1
        LSR.L   D1,D3
        OR.L    D3,D0
        AND.L   D4,D0
.done:  CMP.W   #16,D2
        BNE     .out
        EXT.L   D0
.out:   MOVE.L  (SP)+,D4
        RTS

rt_swap:
        SWAP    D0
        RTS
rt_word:
        EXT.L   D0
        RTS
rt_card:
        AND.L   #$FFFF,D0
        RTS
rt_byte:
        AND.L   #$FF,D0
        RTS
