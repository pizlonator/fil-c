  .text
  .global sym
sym: nop

  .section .data,"aw"
  .dword sym

  .section .discard,"aw"
  .dword sym
