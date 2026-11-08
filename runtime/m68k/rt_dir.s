;! module    rt_dir
;! provides  rt_mkdir rt_chdir rt_files
;! requires  rt_error rt_print_str rt_print_nl plat_mkdir plat_chdir plat_dir_open plat_dir_next plat_fopen plat_fwrite plat_fclose
;! group     file-seq
;! clobbers  D0-D3/A0-A1
;! doc       MKDIR, CHDIR, FILES/DIR [TO name$] (probe25 T3-T5); RMDIR is rt_file_kill (DeleteFile).

; FILES: per entry " name" (a directory "*name"), the size right-aligned up to column 39, then
; " hh:mm:ss dd.mm.yyyy" (probe25 T4); DIR: the names of the files only. The order is that of the
; platform (AmigaDOS: ExNext).

rt_dir__BUF EQU 160

; A0 = descriptor of the name
rt_mkdir:
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        MOVE.L  (A0),A0
        JSR     plat_mkdir
        BRA     rt_dir__check

rt_chdir:
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        MOVE.L  (A0),A0
        JSR     plat_chdir
rt_dir__check:
        TST.L   D0
        BEQ     .out
        JMP     rt_error
.out:   RTS

; A0 = descriptor of the path, A1 = descriptor of the file of TO (length 0: the screen), D0 = 1 FILES,
; 0 DIR
rt_files:
        MOVEM.L D4-D7/A2-A5,-(SP)
        MOVE.L  D0,D7                   ; D7 = FILES
        MOVE.L  A1,A4                   ; A4 = TO
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        MOVE.L  (A0),A0
        JSR     plat_dir_open
        TST.L   D0
        BNE     .err
        MOVEQ   #0,D6                   ; D6 = handle of the file (0: the screen)
        TST.W   4(A4)
        BEQ     .next
        MOVEQ   #0,D0
        MOVE.W  4(A4),D0
        MOVE.L  (A4),A0
        MOVEQ   #1,D1                   ; write
        JSR     plat_fopen
        MOVE.L  D0,D6
        BNE     .next
        MOVE.L  D1,D0
        BRA     .err
.next:  JSR     plat_dir_next
        TST.L   D0
        BEQ     .end
        TST.L   D7
        BNE     .full
        TST.L   D3                      ; DIR: files only
        BNE     .next
        LEA     rt_dir__buf,A3
        BSR     rt_dir__name
        BRA     .put
.full:  MOVE.L  A1,A5                   ; A5 = date
        MOVE.L  D2,D5                   ; D5 = size
        LEA     rt_dir__buf,A3
        MOVE.B  #' ',(A3)+
        TST.L   D3
        BEQ     .nm
        MOVE.B  #'*',-1(A3)
.nm:    BSR     rt_dir__name
        MOVE.L  D5,D0                   ; the digits of the size
        LEA     rt_dir__num+12,A2
.dg:    BSR     rt_dir__div10
        ADD.B   #'0',D1
        MOVE.B  D1,-(A2)
        TST.L   D0
        BNE     .dg
        LEA     rt_dir__num+12,A0
        SUB.L   A2,A0
        MOVE.L  A0,D4                   ; D4 = number of digits
        MOVE.L  A3,D0                   ; column so far
        SUB.L   #rt_dir__buf,D0
        ADD.L   D4,D0
        MOVEQ   #39,D1
        SUB.L   D0,D1                   ; spaces up to column 39, at least one
        BGT     .sp
        MOVEQ   #1,D1
        BRA     .sp
.s:     MOVE.B  #' ',(A3)+
.sp:    SUBQ.L  #1,D1
        BPL     .s
        BRA     .dn
.d:     MOVE.B  (A2)+,(A3)+
.dn:    SUBQ.L  #1,D4
        BPL     .d
        BSR     rt_dir__stamp
.put:   MOVE.L  A3,D1                   ; the line
        LEA     rt_dir__buf,A0
        SUB.L   A0,D1
        TST.L   D6
        BNE     .file
        MOVE.L  D1,D0
        JSR     rt_print_str
        JSR     rt_print_nl
        BRA     .next
.file:  MOVE.B  #10,(A3)
        ADDQ.L  #1,D1
        MOVE.L  D6,D0
        JSR     plat_fwrite
        BRA     .next
.end:   TST.L   D6
        BEQ     .out
        MOVE.L  D6,D0
        JSR     plat_fclose
.out:   MOVEM.L (SP)+,D4-D7/A2-A5
        RTS
.err:   MOVEM.L (SP)+,D4-D7/A2-A5
        JMP     rt_error

; A0 = name (C string), D1 = length, A3 = where -> A3 after it (at most 100 characters)
rt_dir__name:
        CMP.L   #100,D1
        BLE     .n
        MOVEQ   #100,D1
        BRA     .n
.c:     MOVE.B  (A0)+,(A3)+
.n:     SUBQ.L  #1,D1
        BPL     .c
        RTS

; D0 = unsigned value -> D0 = D0 / 10, D1 = remainder (uses D2, D3)
rt_dir__div10:
        MOVE.L  D0,D2
        CLR.W   D0
        SWAP    D0
        DIVU    #10,D0                  ; remainder:quotient of the high word
        MOVE.W  D0,D3
        MOVE.W  D2,D0                   ; remainder:low word
        DIVU    #10,D0
        SWAP    D3
        MOVE.W  D0,D3                   ; D3 = quotient
        CLR.W   D0
        SWAP    D0
        MOVE.L  D0,D1
        MOVE.L  D3,D0
        RTS

; A5 = DateStamp (days since 1.1.1978, minutes, ticks), A3 = where: " hh:mm:ss dd.mm.yyyy"
rt_dir__stamp:
        MOVE.B  #' ',(A3)+
        MOVE.L  4(A5),D0                ; minutes
        DIVU    #60,D0
        BSR     rt_dir__two             ; hours
        MOVE.B  #':',(A3)+
        SWAP    D0
        BSR     rt_dir__two             ; minutes
        MOVE.B  #':',(A3)+
        MOVE.L  8(A5),D0                ; ticks: 50 per second
        DIVU    #50,D0
        BSR     rt_dir__two
        MOVE.B  #' ',(A3)+
        MOVE.L  (A5),D0                 ; D0 = days, D2 = year
        MOVE.L  #1978,D2
.y:     MOVE.L  #365,D1
        MOVE.L  D2,D3
        AND.L   #3,D3
        BNE     .yl
        ADDQ.L  #1,D1                   ; a leap year (1978-2099)
.yl:    CMP.L   D1,D0
        BLT     .m
        SUB.L   D1,D0
        ADDQ.L  #1,D2
        BRA     .y
.m:     LEA     rt_dir__mlen,A0         ; D3 = month 1..12
        MOVEQ   #1,D3
.ml:    MOVEQ   #0,D1
        MOVE.B  (A0)+,D1
        CMP.L   #2,D3
        BNE     .mc
        BTST    #0,D2
        BNE     .mc
        BTST    #1,D2
        BNE     .mc
        ADDQ.L  #1,D1                   ; 29 in a leap year
.mc:    CMP.L   D1,D0
        BLT     .day
        SUB.L   D1,D0
        ADDQ.L  #1,D3
        CMP.L   #12,D3
        BLE     .ml
.day:   ADDQ.L  #1,D0
        BSR     rt_dir__two             ; day
        MOVE.B  #'.',(A3)+
        MOVE.L  D3,D0
        BSR     rt_dir__two             ; month
        MOVE.B  #'.',(A3)+
        MOVE.L  D2,D0                   ; year: two by two digits
        DIVU    #100,D0
        BSR     rt_dir__two
        SWAP    D0
        BRA     rt_dir__two

; D0.W = 0..99 -> two digits at A3 (D0 keeps its high word)
rt_dir__two:
        MOVE.L  D0,-(SP)
        AND.L   #$FFFF,D0
        DIVU    #10,D0
        ADD.B   #'0',D0
        MOVE.B  D0,(A3)+
        SWAP    D0
        ADD.B   #'0',D0
        MOVE.B  D0,(A3)+
        MOVE.L  (SP)+,D0
        RTS

;! section data
rt_dir__mlen:
        DC.B    31,28,31,30,31,30,31,31,30,31,30,31

;! section bss
rt_dir__buf:
        DS.B    rt_dir__BUF
rt_dir__num:
        DS.B    12
