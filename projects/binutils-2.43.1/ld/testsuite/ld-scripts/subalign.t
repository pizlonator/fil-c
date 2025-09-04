SECTIONS
{
    . = 0x10024;

    .mysection1 : SUBALIGN(4) {
        *(.mysection1)
    }

    .mysection2 : ALIGN(16) {
        *(.mysection2)
    }
  
    .mysection3 : {
        *(.mysection3)
    }
  
    /DISCARD/ : {
      *(*)
    }
}
