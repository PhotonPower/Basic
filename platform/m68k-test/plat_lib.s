;! module    plat_lib
;! provides  plat_libcall plat_window plat_screen plat_mouse plat_stick plat_strig
;! group     amiga-os
;! clobbers  D0-D3/A0-A1
;! doc       m68k-test: no AmigaOS, no mouse, no joystick: library calls, WINDOW(), SCREEN(), the mouse and the sticks give 0.

plat_libcall:
plat_mouse:
plat_stick:
plat_strig:
plat_window:
plat_screen:
        MOVEQ   #0,D0
        RTS
