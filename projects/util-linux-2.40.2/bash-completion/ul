_ul_module()
{
	local cur prev OPTS
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"
	case $prev in
		'-t'|'--terminal')
			local TERM_LIST I
			TERM_LIST=''
			for I in /usr/share/terminfo/?/*; do
				TERM_LIST+="${I##*/} "
			done
			COMPREPLY=( $(compgen -W "$TERM_LIST" -- $cur) )
			return 0
			;;
		'-h'|'--help'|'-V'|'--version')
			return 0
			;;
	esac
	case $cur in
		-*)
			OPTS="--terminal --indicated --version --help"
			COMPREPLY=( $(compgen -W "${OPTS[*]}" -- $cur) )
			return 0
			;;
	esac
	local IFS=$'\n'
	compopt -o filenames
	COMPREPLY=( $(compgen -f -- $cur) )
	return 0
}
complete -F _ul_module ul
