# verify_fe6298e3.py
word = 0xfe6296e3          # 原始小端 32-bit 指令
# B-type 立即数字段
imm12  = (word >> 31) & 0x1
imm10_5 = (word >> 25) & 0x3f
imm4_1  = (word >>  8) & 0xf
imm11   = (word >>  7) & 0x1
# 拼成 13-bit 有符号数
imm13 = (imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) | (imm4_1 << 1)
print('拼接后的 13-bit 二进制 =', format(imm12 & 0x1FFF, '01b'))
print('拼接后的 13-bit 二进制 =', format(imm10_5 & 0x1FFF, '06b'))
print('拼接后的 13-bit 二进制 =', format(imm4_1 & 0x1FFF, '04b'))
print('拼接后的 13-bit 二进制 =', format(imm13 & 0x1FFF, '013b'))
imm13 = imm13 & 0x1FFF
if imm13 & 0x1000:      # 负数
    imm13 |= ~0x1FFF
print('拼接后的 13-bit 二进制 =', format(imm13, '032b'))
byte_offset = imm13                      # 硬件再左移 1 位，但我们已把 bit0 留 0
print('13-bit encoded imm =', imm13)
print('byte offset        =', byte_offset)
print('match -20?         =', byte_offset == -20)