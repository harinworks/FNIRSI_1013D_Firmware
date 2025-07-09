MEMORY
{
	ram  : org = 0x000000000, len = 32K
}

SECTIONS
{
  . = ORIGIN(ram);
  .text : 
  {
    KEEP(*start.s.o (.text))
    *(.text);
  } >ram

	__bootloader_size = SIZEOF(.text);
}
