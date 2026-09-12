struct link_map_machine
{
  Elf64_Addr plt; /* Address of .plt + 0x2e */
  const Elf64_Rela *jmprel; /* Address of first JMP_SLOT reloc */
};
