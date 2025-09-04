target:

	# reg_list
	cm.push {a0}, -64
	cm.pop {ra, s1}, -64
	cm.popret {ra, s2-s3}, -64
	cm.popretz {ra, s0-s10}, -112

	# spimm
	cm.push {ra}, 0
	cm.pop {ra, s0}, -80
	cm.popret {ra, s0-s1}, -15
	cm.popretz {ra, s0-s11}, -165
