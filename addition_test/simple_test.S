    .text
    .balign 4

LOOP:
    fld     f0, 0(t0)
    fmul.d  f4, f0, f2
    fsd     f4, 0(t0)
    addi    t0, t0, -8
    bne     t0, t1, LOOP