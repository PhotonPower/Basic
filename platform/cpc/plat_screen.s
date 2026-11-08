;! module    plat_screen
;! provides  plat_locate plat_cls
;! group     print
;! clobbers  AF BC DE HL
;! doc       cpc: cursor through TXT SET CURSOR (&BB75), CLS through TXT CLEAR WINDOW (&BB6C).

; DE = column, HL = line (1..32767; the firmware takes up to 255)
plat_locate:
        LD      A,H
        OR      A
        JR      Z,plat_screen__l
        LD      L,255
plat_screen__l:
        LD      A,D
        OR      A
        JR      Z,plat_screen__c
        LD      E,255
plat_screen__c:
        LD      H,E                     ; H = column, L = line
        JP      &BB75

plat_cls:
        JP      &BB6C
