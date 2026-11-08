;! module    plat_lib
;! provides  plat_libcall plat_window plat_screen
;! group     amiga-os
;! clobbers  D0-D3/A0-A1
;! doc       m68k-test: no AmigaOS; library calls, WINDOW() and SCREEN() give 0 (D28).

plat_libcall:
plat_window:
plat_screen:
        MOVEQ   #0,D0
        RTS
