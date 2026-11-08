;! module    rt_errtext
;! provides  rt_err_text
;! requires  rt_str_newtemp
;! group     string
;! clobbers  AF BC DE HL
;! doc       ERR$(n): the texts of GFA-BASIC 3.5 (English, read on the original, probe1-probe9).

; DE -> error number (only the low byte counts: ERR$(-56) = "unknown error 200") -> HL = temporary
rt_err_text:
        LD      A,(DE)
        LD      (rt_errtext__n),A
        CP      36
        JR      NC,rt_errtext__unk
        LD      L,A
        LD      H,0
        ADD     HL,HL
        LD      DE,rt_errtext__tab
        ADD     HL,DE
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A                     ; HL = text: length byte, characters
        JR      rt_errtext__copy
rt_errtext__unk:                        ; further texts seen on the original
        LD      HL,rt_errtext__more
rt_errtext__mo:
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        BIT     7,D
        JR      NZ,rt_errtext__num
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        LD      A,(rt_errtext__n)
        CP      E
        JR      NZ,rt_errtext__mo
        LD      A,D
        OR      A
        JR      NZ,rt_errtext__mo
        LD      H,B
        LD      L,C
        JR      rt_errtext__copy
rt_errtext__num:                        ; "unknown error " + number (1-3 digits)
        LD      HL,rt_errtext__t7+1
        LD      DE,rt_errtext__buf
        LD      BC,14
        LDIR
        LD      A,(rt_errtext__n)
        LD      C,0                     ; hundreds, tens, ones (no leading zeros)
        LD      B,100
        CALL    rt_errtext__dig
        LD      B,10
        CALL    rt_errtext__dig
        ADD     A,'0'
        LD      (DE),A
        INC     DE
        LD      HL,rt_errtext__buf
        EX      DE,HL
        OR      A
        SBC     HL,DE                   ; length
        LD      A,L
        LD      (rt_errtext__buf-1),A
        LD      HL,rt_errtext__buf-1
        JR      rt_errtext__copy
rt_errtext__dig:                        ; A = value, B = 100 or 10, C = 1 after the first digit
        LD      H,'0'-1
rt_errtext__d1:
        INC     H
        SUB     B
        JR      NC,rt_errtext__d1
        ADD     A,B
        PUSH    AF
        LD      A,H
        CP      '0'
        JR      NZ,rt_errtext__d2
        LD      A,C
        OR      A
        JR      Z,rt_errtext__d3
rt_errtext__d2:
        LD      A,H
        LD      (DE),A
        INC     DE
        LD      C,1
rt_errtext__d3:
        POP     AF
        RET

; HL = length byte, characters -> HL = new temporary with them
rt_errtext__copy:
        LD      C,(HL)
        LD      B,0
        INC     HL
        PUSH    HL
        PUSH    BC
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        POP     BC
        EX      (SP),HL                 ; HL = source, stack = descriptor
        LD      A,B
        OR      C
        JR      Z,rt_errtext__c9
        LDIR
rt_errtext__c9:
        POP     HL
        RET

;! section data
rt_errtext__more:
        DW      45
        DW      rt_errtext__t45
        DW      67
        DW      rt_errtext__t67
        DW      68
        DW      rt_errtext__t68
        DW      202
        DW      rt_errtext__t202
        DW      205
        DW      rt_errtext__t205
        DW      50
        DW      rt_errtext__t50
        DW      54
        DW      rt_errtext__t54
        DW      55
        DW      rt_errtext__t55
        DW      203
        DW      rt_errtext__t203
        DW      219
        DW      rt_errtext__t219
        DW      77
        DW      rt_errtext__t77
        DW      79
        DW      rt_errtext__t79
        DW      82
        DW      rt_errtext__t82
        DW      83
        DW      rt_errtext__t83
        DW      -1
rt_errtext__t77:
        DB      31,"Matrices are of different order"
rt_errtext__t79:
        DB      26,"Matrix product not defined"
rt_errtext__t82:
        DB      17,"Non square matrix"
rt_errtext__t83:
        DB      25,"Transposition not defined"
rt_errtext__t45:
        DB      19,"too many parameters"
rt_errtext__t67:
        DB      15,"asin/acos error"
rt_errtext__t68:
        DB      17,"var-type mismatch"
rt_errtext__t50:
        DB      26,102,105,101,108,100,115,32,103,114,101,97,116,101,114,32,116,104,97,110,32,114,101,99,111,114,100
rt_errtext__t54:
        DB      32,103,101,116,47,112,117,116,32,102,105,101,108,100,32,115,116,114,105,110,103,124,108,101,110,32,99,104,97,110,103,101,100
rt_errtext__t55:
        DB      29,103,101,116,47,112,117,116,58,32,105,108,108,101,103,97,108,32,114,101,99,111,114,100,110,117,109,98,101,114
rt_errtext__t203:
        DB      21,111,98,106,101,99,116,32,97,108,114,101,97,100,121,32,101,120,105,115,116,115
rt_errtext__t219:
        DB      10,115,101,101,107,32,101,114,114,111,114
rt_errtext__t202:
        DB      13,"object in use"
rt_errtext__t205:
        DB      16,"object not found"
rt_errtext__tab:
        DW      rt_errtext__t0
        DW      rt_errtext__t1
        DW      rt_errtext__t2
        DW      rt_errtext__t3
        DW      rt_errtext__t4
        DW      rt_errtext__t5
        DW      rt_errtext__t6
        DW      rt_errtext__t7
        DW      rt_errtext__t8
        DW      rt_errtext__t9
        DW      rt_errtext__t10
        DW      rt_errtext__t11
        DW      rt_errtext__t12
        DW      rt_errtext__t13
        DW      rt_errtext__t14
        DW      rt_errtext__t15
        DW      rt_errtext__t16
        DW      rt_errtext__t17
        DW      rt_errtext__t18
        DW      rt_errtext__t19
        DW      rt_errtext__t20
        DW      rt_errtext__t21
        DW      rt_errtext__t22
        DW      rt_errtext__t23
        DW      rt_errtext__t24
        DW      rt_errtext__t25
        DW      rt_errtext__t26
        DW      rt_errtext__t27
        DW      rt_errtext__t28
        DW      rt_errtext__t29
        DW      rt_errtext__t30
        DW      rt_errtext__t31
        DW      rt_errtext__t32
        DW      rt_errtext__t33
        DW      rt_errtext__t34
        DW      rt_errtext__t35
rt_errtext__t0:
        DB      14,100,105,118,105,100,101,32,98,121,32,122,101,114,111
rt_errtext__t1:
        DB      8,111,118,101,114,102,108,111,119
rt_errtext__t2:
        DB      36,110,111,32,105,110,116,101,103,101,114,124,45,50,49,52,55,52,56,51,54,52,56,32,46,46,32,50,49,52,55,52,56,51,54,52,55
rt_errtext__t3:
        DB      16,110,111,32,98,121,116,101,124,48,32,46,46,32,50,53,53
rt_errtext__t4:
        DB      23,110,111,32,119,111,114,100,124,45,51,50,55,54,56,32,46,46,32,51,50,55,54,55
rt_errtext__t5:
        DB      22,110,111,32,110,101,103,97,116,105,118,101,32,115,113,117,97,114,101,114,111,111,116
rt_errtext__t6:
        DB      39,108,111,103,97,114,105,116,104,109,32,111,110,108,121,32,102,111,114,124,110,117,109,98,101,114,115,32,103,114,101,97,116,101,114,32,122,101,114,111
rt_errtext__t7:
        DB      14,117,110,107,110,111,119,110,32,101,114,114,111,114,32
rt_errtext__t8:
        DB      13,111,117,116,32,111,102,32,109,101,109,111,114,121
rt_errtext__t9:
        DB      35,102,117,110,99,116,105,111,110,32,111,114,32,99,111,109,109,97,110,100,124,110,111,116,32,105,109,112,108,101,109,101,110,116,101,100
rt_errtext__t10:
        DB      32,115,116,114,105,110,103,32,116,111,111,32,108,111,110,103,124,109,97,120,46,32,51,50,55,54,55,32,99,104,97,114,115
rt_errtext__t11:
        DB      24,110,111,32,71,70,65,45,66,65,83,73,67,32,51,46,53,32,112,114,111,103,114,97,109
rt_errtext__t12:
        DB      34,112,114,111,103,114,97,109,32,116,111,111,32,108,111,110,103,124,111,117,116,32,111,102,32,109,101,109,111,114,121,124,110,101,119
rt_errtext__t13:
        DB      30,110,111,32,71,70,65,45,66,65,83,73,67,32,112,114,111,103,114,97,109,124,101,111,102,32,45,32,110,101,119
rt_errtext__t14:
        DB      19,114,101,100,105,109,101,110,115,105,111,110,101,100,32,97,114,114,97,121
rt_errtext__t15:
        DB      19,97,114,114,97,121,32,117,110,100,105,109,101,110,115,105,111,110,101,100
rt_errtext__t16:
        DB      24,97,114,114,97,121,32,105,110,100,101,120,32,111,117,116,32,111,102,32,114,97,110,103,101
rt_errtext__t17:
        DB      16,100,105,109,32,111,117,116,32,111,102,32,114,97,110,103,101
rt_errtext__t18:
        DB      18,119,114,111,110,103,32,35,32,111,102,32,105,110,100,105,99,101,115
rt_errtext__t19:
        DB      19,112,114,111,99,101,100,117,114,101,32,110,111,116,32,102,111,117,110,100
rt_errtext__t20:
        DB      15,108,97,98,101,108,32,110,111,116,32,102,111,117,110,100
rt_errtext__t21:
        DB      57,111,112,101,110,32,97,108,108,111,119,115,58,124,34,105,34,110,112,117,116,32,32,34,111,34,117,116,112,117,116,32,34,114,34,97,110,100,111,109,124,34,97,34,112,112,101,110,100,32,34,117,34,112,100,97,116,101
rt_errtext__t22:
        DB      17,102,105,108,101,32,97,108,114,101,97,100,121,32,111,112,101,110
rt_errtext__t23:
        DB      12,102,105,108,101,32,35,32,119,114,111,110,103
rt_errtext__t24:
        DB      13,102,105,108,101,32,110,111,116,32,111,112,101,110
rt_errtext__t25:
        DB      22,119,114,111,110,103,32,105,110,112,117,116,44,32,110,111,32,110,117,109,98,101,114
rt_errtext__t26:
        DB      11,101,110,100,32,111,102,32,102,105,108,101
rt_errtext__t27:
        DB      46,116,111,111,32,109,97,110,121,32,112,111,105,110,116,115,32,102,111,114,124,112,111,108,121,108,105,110,101,47,112,111,108,121,102,105,108,108,124,109,97,120,46,32,49,50,56
rt_errtext__t28:
        DB      16,97,114,114,97,121,32,110,111,116,32,108,105,110,101,97,114
rt_errtext__t29:
        DB      21,35,32,111,102,32,112,111,105,110,116,115,32,116,111,111,32,108,97,114,103,101
rt_errtext__t30:
        DB      16,109,101,114,103,101,32,45,32,110,111,32,65,83,67,73,73
rt_errtext__t31:
        DB      21,109,101,114,103,101,32,45,32,108,105,110,101,32,116,111,111,32,108,111,110,103
rt_errtext__t32:
        DB      32,61,61,62,32,115,121,110,116,97,120,32,101,114,114,111,114,124,112,114,111,103,114,97,109,32,97,98,111,114,116,101,100
rt_errtext__t33:
        DB      15,117,110,100,101,102,105,110,101,100,32,108,97,98,101,108
rt_errtext__t34:
        DB      11,111,117,116,32,111,102,32,100,97,116,97
rt_errtext__t35:
        DB      16,100,97,116,97,32,110,111,116,32,110,117,109,101,114,105,99

;! section bss
rt_errtext__n:
        DS      1
rt_errtext__len:
        DS      1
rt_errtext__buf:
        DS      20
