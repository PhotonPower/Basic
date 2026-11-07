;! module    rt_rnd
;! provides  rt_randomize rt_rnd_next rt_rnd rt_rand
;! requires  fp_round_pack
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Random numbers: xorshift32 (Marsaglia 13, 17, 5), the same on all targets.

; rt_randomize: D0 = seed (0: fixed constant). rt_rnd_next: -> D0 = new state.
; rt_rnd: -> D0:D1 = state / 2^32. rt_rand: D0 = y -> D0 = (state >> 16) * (y AND $FFFF) >> 16.

rt_randomize:
        TST.L   D0
        BNE     .set
        MOVE.L  #$2545F491,D0
.set:   MOVE.L  D0,rt_rnd__state
        RTS

rt_rnd_next:
        MOVE.L  D1,-(SP)
        MOVE.L  rt_rnd__state,D0
        MOVE.L  D0,D1                   ; x ^= x << 13
        MOVEQ   #13,D2
        LSL.L   D2,D1
        EOR.L   D1,D0
        MOVE.L  D0,D1                   ; x ^= x >> 17
        MOVEQ   #17,D2
        LSR.L   D2,D1
        EOR.L   D1,D0
        MOVE.L  D0,D1                   ; x ^= x << 5
        LSL.L   #5,D1
        EOR.L   D1,D0
        MOVE.L  D0,rt_rnd__state
        MOVE.L  (SP)+,D1
        RTS

rt_rnd:
        BSR     rt_rnd_next
        MOVE.L  D0,D1                   ; m = 0:state, e = -32 (exact)
        MOVEQ   #0,D0
        MOVEQ   #-32,D2
        MOVEQ   #0,D3
        JMP     fp_round_pack

rt_rand:
        MOVE.L  D0,D3
        BSR     rt_rnd_next
        SWAP    D0                      ; state >> 16 in the low word
        MULU    D3,D0                   ; * (y AND $FFFF)
        CLR.W   D0
        SWAP    D0                      ; >> 16
        RTS

;! section data
rt_rnd__state:
        DC.L    $2545F491
