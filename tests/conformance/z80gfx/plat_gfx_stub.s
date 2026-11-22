;! module    plat_gfx_stub
;! provides  plat_gfx_plot plat_gfx_move plat_gfx_draw plat_gfx_test plat_gfx_pen plat_gfx_paper plat_gfx_mode
;! provides  plat_gfx_ink plat_gfx_text plat_gfx_clear plat_gfx_lmask stub_ink_pen stub_ink_rgb
;! group     graphics
;! doc       Test stub of the CPC graphics platform: does nothing, but records the last plat_gfx_ink (pen, 0RGB).

plat_gfx_ink:
        LD      (stub_ink_pen),A
        LD      (stub_ink_rgb),HL
plat_gfx_plot:
plat_gfx_move:
plat_gfx_draw:
plat_gfx_test:
plat_gfx_pen:
plat_gfx_paper:
plat_gfx_mode:
plat_gfx_text:
plat_gfx_clear:
plat_gfx_lmask:
        RET

;! section data
stub_ink_pen:
        DB      255
stub_ink_rgb:
        DW      65535
