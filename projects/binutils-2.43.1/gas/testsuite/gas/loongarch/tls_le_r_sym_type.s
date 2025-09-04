f:
  lu12i.w	$a0, %le_hi20_r(x)
  add.d		$a0, $a0, $tp, %le_add_r(x)
  li.w		$a1, 1
  st.w		$a1, $a0, %le_lo12_r(x)
  ret
