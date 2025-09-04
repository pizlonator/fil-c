.text
a:
  ret
  ret
  ret
b:
  ret
  .fill 0x7fffff0

  pcaddu18i $ra, %call36(a) # min offset, can relax
  jirl	    $ra, $ra, 0
  pcaddu18i $ra, %call36(a) # overflow, not relax
  jirl	    $ra, $ra, 0
  pcaddu18i $t0, %call36(b) # min offset, can relax
  jirl	    $zero, $t0, 0
  pcaddu18i $t0, %call36(b) # overflow, not relax
  jirl	    $zero, $t0, 0

  pcaddu18i $ra, %call36(c) # overflow, not relax
  jirl	    $ra, $ra, 0
  pcaddu18i $ra, %call36(c) # max offset, can relax
  jirl	    $ra, $ra, 0
  pcaddu18i $t0, %call36(d) # overflow, no relax
  jirl	    $zero, $t0, 0
  pcaddu18i $t0, %call36(d) # max offset, can relax
  jirl	    $zero, $t0, 0

  .fill 0x7ffffc8
  ret
  ret
  ret
  ret
  ret
  ret
  ret
  ret
c:
  ret
  ret
  ret
d:
  ret

