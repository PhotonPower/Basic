;! module    rt_errtext
;! provides  rt_err_text
;! requires  rt_str_newtemp
;! group     string
;! clobbers  D0-D3/A0-A1
;! doc       ERR$(n): the texts of GFA-BASIC 3.5 (English, read on the original, probe1-probe9).

; D0 = error number (only the low byte counts: ERR$(-56) = "unknown error 200") -> A0 = temporary
rt_err_text:
        AND.L   #$FF,D0
        CMP.L   #36,D0
        BCC     .unk
        LSL.L   #2,D0
        LEA     rt_errtext__tab,A0
        MOVE.L  0(A0,D0.L),A0           ; A0 = text: length byte, characters
        MOVEQ   #0,D0
        MOVE.B  (A0)+,D0
        BRA     rt_errtext__copy
.unk:   LEA     rt_errtext__more,A0     ; further texts seen on the original
.mo:    MOVE.W  (A0)+,D1
        BMI     .num
        MOVE.L  (A0)+,A1
        CMP.W   D0,D1
        BNE     .mo
        MOVE.L  A1,A0
        MOVEQ   #0,D0
        MOVE.B  (A0)+,D0
        BRA     rt_errtext__copy
.num:   LINK    A6,#-20                 ; "unknown error " + number (1-3 digits)
        LEA     -20(A6),A1
        LEA     rt_errtext__t7+1,A0
        MOVEQ   #13,D1
.c:     MOVE.B  (A0)+,(A1)+
        DBRA    D1,.c
        MOVE.L  D0,D1                   ; digits from the end
        LEA     -3(A6),A0
        MOVEQ   #0,D2                   ; D2 = digit count
.d:     DIVU    #10,D1
        SWAP    D1
        ADD.B   #'0',D1
        MOVE.B  D1,-(A0)
        CLR.W   D1
        SWAP    D1
        ADDQ.L  #1,D2
        TST.W   D1
        BNE     .d
.m:     MOVE.B  (A0)+,(A1)+             ; move the digits behind the text
        SUBQ.L  #1,D2
        BNE     .m
        LEA     -20(A6),A0
        MOVE.L  A1,D0
        SUB.L   A0,D0
        BSR     rt_errtext__copy
        UNLK    A6
        RTS

; A0 = characters, D0 = length -> A0 = new temporary with them
rt_errtext__copy:
        MOVEM.L D0/A0,-(SP)
        JSR     rt_str_newtemp          ; A1 = descriptor, A0 = its characters
        MOVE.L  A1,D1
        MOVEM.L (SP)+,D0/A1             ; D0 = length, A1 = source
        BRA     .n
.l:     MOVE.B  (A1)+,(A0)+
.n:     SUBQ.L  #1,D0
        BPL     .l
        MOVE.L  D1,A0
        RTS

; (number, text) for numbers above 35, ended by -1
rt_errtext__more:
        DC.W    45
        DC.L    rt_errtext__t45
        DC.W    67
        DC.L    rt_errtext__t67
        DC.W    68
        DC.L    rt_errtext__t68
        DC.W    202
        DC.L    rt_errtext__t202
        DC.W    205
        DC.L    rt_errtext__t205
        DC.W    50
        DC.L    rt_errtext__t50
        DC.W    54
        DC.L    rt_errtext__t54
        DC.W    55
        DC.L    rt_errtext__t55
        DC.W    203
        DC.L    rt_errtext__t203
        DC.W    219
        DC.L    rt_errtext__t219
        DC.W    -1
rt_errtext__t45:
        DC.B    19,'too many parameters'
rt_errtext__t67:
        DC.B    15,'asin/acos error'
rt_errtext__t68:
        DC.B    17,'var-type mismatch'
rt_errtext__t50:
        DC.B    26,102,105,101,108,100,115,32,103,114,101,97,116,101,114,32,116,104,97,110,32,114,101,99,111,114,100
rt_errtext__t54:
        DC.B    32,103,101,116,47,112,117,116,32,102,105,101,108,100,32,115,116,114,105,110,103,124,108,101,110,32,99,104,97,110,103,101,100
rt_errtext__t55:
        DC.B    29,103,101,116,47,112,117,116,58,32,105,108,108,101,103,97,108,32,114,101,99,111,114,100,110,117,109,98,101,114
rt_errtext__t203:
        DC.B    21,111,98,106,101,99,116,32,97,108,114,101,97,100,121,32,101,120,105,115,116,115
rt_errtext__t219:
        DC.B    10,115,101,101,107,32,101,114,114,111,114
rt_errtext__t202:
        DC.B    13,'object in use'
rt_errtext__t205:
        DC.B    16,'object not found'
        EVEN

rt_errtext__tab:
        DC.L    rt_errtext__t0
        DC.L    rt_errtext__t1
        DC.L    rt_errtext__t2
        DC.L    rt_errtext__t3
        DC.L    rt_errtext__t4
        DC.L    rt_errtext__t5
        DC.L    rt_errtext__t6
        DC.L    rt_errtext__t7
        DC.L    rt_errtext__t8
        DC.L    rt_errtext__t9
        DC.L    rt_errtext__t10
        DC.L    rt_errtext__t11
        DC.L    rt_errtext__t12
        DC.L    rt_errtext__t13
        DC.L    rt_errtext__t14
        DC.L    rt_errtext__t15
        DC.L    rt_errtext__t16
        DC.L    rt_errtext__t17
        DC.L    rt_errtext__t18
        DC.L    rt_errtext__t19
        DC.L    rt_errtext__t20
        DC.L    rt_errtext__t21
        DC.L    rt_errtext__t22
        DC.L    rt_errtext__t23
        DC.L    rt_errtext__t24
        DC.L    rt_errtext__t25
        DC.L    rt_errtext__t26
        DC.L    rt_errtext__t27
        DC.L    rt_errtext__t28
        DC.L    rt_errtext__t29
        DC.L    rt_errtext__t30
        DC.L    rt_errtext__t31
        DC.L    rt_errtext__t32
        DC.L    rt_errtext__t33
        DC.L    rt_errtext__t34
        DC.L    rt_errtext__t35
rt_errtext__t0:
        DC.B    14,100,105,118,105,100,101,32,98,121,32,122,101,114,111
rt_errtext__t1:
        DC.B    8,111,118,101,114,102,108,111,119
rt_errtext__t2:
        DC.B    36,110,111,32,105,110,116,101,103,101,114,124,45,50,49,52,55,52,56,51,54,52,56,32,46,46,32,50,49,52,55,52,56,51,54,52,55
rt_errtext__t3:
        DC.B    16,110,111,32,98,121,116,101,124,48,32,46,46,32,50,53,53
rt_errtext__t4:
        DC.B    23,110,111,32,119,111,114,100,124,45,51,50,55,54,56,32,46,46,32,51,50,55,54,55
rt_errtext__t5:
        DC.B    22,110,111,32,110,101,103,97,116,105,118,101,32,115,113,117,97,114,101,114,111,111,116
rt_errtext__t6:
        DC.B    39,108,111,103,97,114,105,116,104,109,32,111,110,108,121,32,102,111,114,124,110,117,109,98,101,114,115,32,103,114,101,97,116,101,114,32,122,101,114,111
rt_errtext__t7:
        DC.B    14,117,110,107,110,111,119,110,32,101,114,114,111,114,32
rt_errtext__t8:
        DC.B    13,111,117,116,32,111,102,32,109,101,109,111,114,121
rt_errtext__t9:
        DC.B    35,102,117,110,99,116,105,111,110,32,111,114,32,99,111,109,109,97,110,100,124,110,111,116,32,105,109,112,108,101,109,101,110,116,101,100
rt_errtext__t10:
        DC.B    32,115,116,114,105,110,103,32,116,111,111,32,108,111,110,103,124,109,97,120,46,32,51,50,55,54,55,32,99,104,97,114,115
rt_errtext__t11:
        DC.B    24,110,111,32,71,70,65,45,66,65,83,73,67,32,51,46,53,32,112,114,111,103,114,97,109
rt_errtext__t12:
        DC.B    34,112,114,111,103,114,97,109,32,116,111,111,32,108,111,110,103,124,111,117,116,32,111,102,32,109,101,109,111,114,121,124,110,101,119
rt_errtext__t13:
        DC.B    30,110,111,32,71,70,65,45,66,65,83,73,67,32,112,114,111,103,114,97,109,124,101,111,102,32,45,32,110,101,119
rt_errtext__t14:
        DC.B    19,114,101,100,105,109,101,110,115,105,111,110,101,100,32,97,114,114,97,121
rt_errtext__t15:
        DC.B    19,97,114,114,97,121,32,117,110,100,105,109,101,110,115,105,111,110,101,100
rt_errtext__t16:
        DC.B    24,97,114,114,97,121,32,105,110,100,101,120,32,111,117,116,32,111,102,32,114,97,110,103,101
rt_errtext__t17:
        DC.B    16,100,105,109,32,111,117,116,32,111,102,32,114,97,110,103,101
rt_errtext__t18:
        DC.B    18,119,114,111,110,103,32,35,32,111,102,32,105,110,100,105,99,101,115
rt_errtext__t19:
        DC.B    19,112,114,111,99,101,100,117,114,101,32,110,111,116,32,102,111,117,110,100
rt_errtext__t20:
        DC.B    15,108,97,98,101,108,32,110,111,116,32,102,111,117,110,100
rt_errtext__t21:
        DC.B    57,111,112,101,110,32,97,108,108,111,119,115,58,124,34,105,34,110,112,117,116,32,32,34,111,34,117,116,112,117,116,32,34,114,34,97,110,100,111,109,124,34,97,34,112,112,101,110,100,32,34,117,34,112,100,97,116,101
rt_errtext__t22:
        DC.B    17,102,105,108,101,32,97,108,114,101,97,100,121,32,111,112,101,110
rt_errtext__t23:
        DC.B    12,102,105,108,101,32,35,32,119,114,111,110,103
rt_errtext__t24:
        DC.B    13,102,105,108,101,32,110,111,116,32,111,112,101,110
rt_errtext__t25:
        DC.B    22,119,114,111,110,103,32,105,110,112,117,116,44,32,110,111,32,110,117,109,98,101,114
rt_errtext__t26:
        DC.B    11,101,110,100,32,111,102,32,102,105,108,101
rt_errtext__t27:
        DC.B    46,116,111,111,32,109,97,110,121,32,112,111,105,110,116,115,32,102,111,114,124,112,111,108,121,108,105,110,101,47,112,111,108,121,102,105,108,108,124,109,97,120,46,32,49,50,56
rt_errtext__t28:
        DC.B    16,97,114,114,97,121,32,110,111,116,32,108,105,110,101,97,114
rt_errtext__t29:
        DC.B    21,35,32,111,102,32,112,111,105,110,116,115,32,116,111,111,32,108,97,114,103,101
rt_errtext__t30:
        DC.B    16,109,101,114,103,101,32,45,32,110,111,32,65,83,67,73,73
rt_errtext__t31:
        DC.B    21,109,101,114,103,101,32,45,32,108,105,110,101,32,116,111,111,32,108,111,110,103
rt_errtext__t32:
        DC.B    32,61,61,62,32,115,121,110,116,97,120,32,101,114,114,111,114,124,112,114,111,103,114,97,109,32,97,98,111,114,116,101,100
rt_errtext__t33:
        DC.B    15,117,110,100,101,102,105,110,101,100,32,108,97,98,101,108
rt_errtext__t34:
        DC.B    11,111,117,116,32,111,102,32,100,97,116,97
rt_errtext__t35:
        DC.B    16,100,97,116,97,32,110,111,116,32,110,117,109,101,114,105,99
        EVEN
