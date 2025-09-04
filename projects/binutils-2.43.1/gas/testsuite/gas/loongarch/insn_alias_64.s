L1:
  move  $a0,$a1
  li.w  $a0,-1
  li.d  $a0,-1
  nop
  li.w	$a0,1
  ret
  jr    $ra
  bltz  $a0,.L1
  bgez  $a0,.L1
  blez  $a0,.L1
