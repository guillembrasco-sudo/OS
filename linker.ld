OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(start)

KERNEL_VMA = 0xFFFFFFFF80000000;

SECTIONS
{
	. = KERNEL_VMA + 0x00100000;

	.multiboot ALIGN(8) : AT(ADDR(.multiboot) - KERNEL_VMA)
	{
		KEEP(*(.multiboot))
	}

	.text ALIGN(0x1000) : AT(ADDR(.text) - KERNEL_VMA)
	{
		*(.text .text.*)
	}

	.rodata ALIGN(0x1000) : AT(ADDR(.rodata) - KERNEL_VMA)
	{
		*(.rodata .rodata.*)
	}

	.__ex_table ALIGN(8) : AT(ADDR(.__ex_table) - KERNEL_VMA)
	{
		__start___ex_table = .;
		KEEP(*(__ex_table))
		__stop___ex_table = .;
	}

	.data ALIGN(0x1000) : AT(ADDR(.data) - KERNEL_VMA)
	{
		*(.data .data.*)
	}

	.bss ALIGN(0x1000) : AT(ADDR(.bss) - KERNEL_VMA)
	{
		*(COMMON)
		*(.bss .bss.*)
	}

	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note .note.*)
	}
}
