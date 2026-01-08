ENTRY(_start)
SECTIONS
{
    . = 0x0;          /* 代码从地址 0 开始 */
    .text : { *(.text) }
    .data : { *(.data) }  /* 数据紧跟代码后 */
    .bss : { *(.bss) }
}
