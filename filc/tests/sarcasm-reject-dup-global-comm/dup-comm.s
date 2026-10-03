# Duplicate data-object definition: two `.comm` directives define the same
# name. Sarcasm makes commons strong definitions (they lose cross-module
# merging), so the second definition collides with the first and is
# rejected, reporting the first definition's location.
	.comm	glob1, 16
	.comm	glob1, 16
