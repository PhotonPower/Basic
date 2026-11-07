;! module    plat_getc
;! provides  plat_getc
;! requires  plat_dosbase plat_in plat_flush
;! group     core
;! clobbers  D0-D1/A0-A1
;! doc       amiga: next byte from Input() (dos Read); the output is written first, so that a prompt shows.

; -> D0 = character (0..255), or -1 at the end of the input
plat_getc:
        JSR     plat_flush
        MOVEM.L D2-D3/A6,-(SP)
        MOVE.L  plat_in,D1
        BEQ     .end
        MOVE.L  #plat_getc__c,D2
        MOVEQ   #1,D3
        MOVE.L  plat_dosbase,A6
        JSR     -42(A6)                 ; Read(Input(), &c, 1)
        CMP.L   #1,D0
        BNE     .end
        MOVEQ   #0,D0
        MOVE.B  plat_getc__c,D0
        BRA     .out
.end:   MOVEQ   #-1,D0
.out:   MOVEM.L (SP)+,D2-D3/A6
        RTS

;! section bss
plat_getc__c:
        DS.B    2
