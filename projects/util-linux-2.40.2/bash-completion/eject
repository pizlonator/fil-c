_eject_module()
{
	local cur prev OPTS
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"
	case $prev in
		'-a'|'--auto'|'-i'|'--manualeject')
			COMPREPLY=( $(compgen -W "off on" -- $cur) )
			return 0
			;;
		'-c'|'--changerslot')
			# FIXME: there must be way to determine slots
			COMPREPLY=( $(compgen -W "slot" -- $cur) )
			return 0
			;;
		'-x'|'--cdspeed')
			COMPREPLY=( $(compgen -W "$(eject --listspeed 2>/dev/null)" -- $cur) )
			return 0
			;;
		'-h'|'--help'|'-V'|'--version')
			return 0
			;;
	esac
	case $cur in
		-*)
			OPTS="--auto
				--changerslot
				--default
				--floppy
				--force
				--manualeject
				--no-unmount
				--no-partitions-unmount
				--noop
				--proc
				--tape
				--cdrom
				--scsi
				--trayclose
				--traytoggle
				--verbose
				--cdspeed
				--listspeed
				--help
				--version"
			COMPREPLY=( $(compgen -W "${OPTS[*]}" -- $cur) )
			return 0
			;;
	esac
	local DEVS
	DEVS="$(for I in /sys/class/block/*/removable; do
		if [ $(cat $I) -ne 0 ]; then
			OLD_IFS=$IFS
			IFS='/';
			ARR=($I)
			echo "/dev/${ARR[4]}"
			IFS=$OLD_IFS
		fi
	done)"
	compopt -o bashdefault -o default
	COMPREPLY=( $(compgen -W "$DEVS" $cur) )
	return 0
}
complete -F _eject_module eject
