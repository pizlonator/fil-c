_raw_module()
{
	local cur prev
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"
	case $prev in
		'-h'|'--help'|'-V'|'--version')
			return 0
			;;
	esac
	case $cur in
		-*)
			local OPTS
			OPTS="--query --all --help --version"
			COMPREPLY=( $(compgen -W "${OPTS[*]}" -- $cur) )
			return 0
			;;
	esac
	COMPREPLY=( $(compgen -W "$(for I in /dev/raw/*; do if [ -e $I ]; then echo $I; fi; done)" -- $cur) )
	return 0
}
complete -F _raw_module raw
