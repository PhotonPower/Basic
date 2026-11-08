;! module    rt_mouse
;! provides  rt_mouse rt_stick rt_strig
;! requires  plat_mouse plat_stick plat_strig
;! group     amiga-os
;! clobbers  D0-D3/A0-A1
;! doc       MOUSEX/MOUSEY/MOUSEK, MOUSE, STICK(p), STRIG(p) (D29): from the platform.

; D0 = 0 x, 1 y, 2 buttons (1 left, 2 right, 4 middle) -> D0
rt_mouse:
        JMP     plat_mouse

; D0 = port -> D0 = direction bits (1 up, 2 down, 4 left, 8 right)
rt_stick:
        JMP     plat_stick

; D0 = port -> D0 = -1 fire button pressed, else 0
rt_strig:
        JMP     plat_strig
