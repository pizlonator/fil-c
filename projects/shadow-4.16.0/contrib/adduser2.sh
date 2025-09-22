#!/bin/bash
#
#  adduser			Interactive user adding program.
#
#  Copyright (C) 1996		Petri Mattila, Prihateam Networks
#				petri@prihateam.fi
#	
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
# Changes:
#	220496	v0.01	Initial version
#	230496	v0.02	More checks, embolden summary
#	240496		Even more checks
#	250496 		Help with ?
#	040596	v0.03	Cleanups
#	050596	v0.04	Bug fixes, expire date checks
#	070596	v0.05	Iso-latin-1 names
#

## Defaults

# default groups
def_group="users"
def_other_groups=""

# default home directory
def_home_dir=/home/users

# default shell
def_shell=/bin/tcsh

# Default expiration date (mm/dd/yy)
def_expire=""

# default dates
def_pwd_min=0
def_pwd_max=90
def_pwd_warn=14
def_pwd_iact=14


# possible UIDs
uid_low=1000
uid_high=64000

# skel directory
skel=/etc/skel

# default mode for home directory
def_mode=711

# Regex, that the login name must meet, only ANSI characters
login_regex='^[0-9a-zA-Z_-]*$'

# Regex, that the user name must meet
# ANSI version
##name_regex='^[0-9a-zA-Z_-\ ]*$'
# ISO-LATIN-1 version
name_regex='^[0-9a-zA-ZÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõöùúûüýþÿ_-\ ]*$'

# set PATH
export PATH="/bin:/sbin:/usr/bin:/usr/sbin"

# Some special characters
case "$TERM" in
   vt*|ansi*|con*|xterm*|linux*)
	S='[1m'	# start embolden
	E='[m'	# end embolden
	;;
   *)	
	S=''
	E=''
	;;
esac


## Functions

check_root() {
	if test "$EUID" -ne 0
	then
		echo "You must be root to run this program."
		exit 1
	fi
}

check_user() {
	local usr pwd uid gid name home sh

	cat /etc/passwd | (
		while IFS=":" read usr pwd uid gid name home sh
		do
			if test "$1" = "${usr}"
			then
				return 1
			fi
		done
		return 0
	)
}

check_group() {
	local read grp pwd gid members

	cat /etc/group | (
		while IFS=":" read grp pwd gid members
		do
			if test "$1" = "${grp}"
			then
				return 1
			fi
		done
		return 0
	)
}

check_other_groups() {
	local grp check IFS

	check="$1"
	IFS=","

	set ${check}
	for grp
	do
		if check_group "${grp}"
		then
			echo "Group ${grp} does not exist."
			return 1
		fi
	done
	return 0
}		

check_uid() {
	local usr pwd uid gid name home sh
	
	cat /etc/passwd | (
		while IFS=":" read usr pwd uid gid name home sh
		do
			if test "$1" = "${uid}"
			then
				return 1
			fi
		done
		return 0
	)
}

read_yn() {
	local ans ynd
	
	ynd="$1"
	
	while :
	do
		read ans	
		case "${ans}" in
		      "") return ${ynd} ;;
		    [nN]) return 1 ;;
		    [yY]) return 0 ;;
		       *) echo -n "Y or N, please ? " ;;
		esac
	done
}

read_login() {
	echo
	while :
	do
		echo -n "Login: ${def_login:+[${def_login}] }"
		read login
		
		if test "${login}" = '?'
		then
			less /etc/passwd
			echo
			continue
		fi

		if test -z "${login}" -a -n "${def_login}"
		then
			login="${def_login}"
			echo "Using ${login}"
			return
		fi
		
		if test "${#login}" -gt 8
		then
			echo "Login must be at most 8 characters long"
			continue
		fi
		
		if test "${#login}" -lt 2
		then
			echo "Login must be at least 2 characters long"
			continue
		fi
		
		if ! expr "${login}" : "${login_regex}" &> /dev/null
		then
			echo "Please use letters, numbers and special characters _-,."
			continue
		fi
		
		if ! check_user "${login}"
		then
			echo "Username ${login} is already in use"
			continue
		fi
		
		def_login="${login}"
		return
	done
}

read_name () {
	echo
	while :
	do
		echo -n "Real name: ${def_name:+[${def_name}] }"
		read name
		
		if test "${name}" = '?'
		then
			less /etc/passwd
			echo
			continue
		fi

		if test -z "${name}" -a -n "${def_name}"
		then
			name="${def_name}"
			echo "Using ${name}"
		fi

		if test "${#name}" -gt 32
		then
			echo "Name should be at most 32 characters long"
			continue
		fi

		if ! expr "${name}" : "${name_regex}" &> /dev/null
		then
			echo "Please use letters, numbers, spaces and special characters ,._-"
			continue
		fi
		
		def_name="${name}"
		return
	done
}

read_home() {
	local x
	
	echo
	while :
	do
		echo -n "Home Directory: [${def_home_dir}/${login}] "
		read home
		
		if test -z "${home}"
		then
			home="${def_home_dir}/${login}"
			echo "Using ${home}"
		fi
		
		if ! expr "${home}" : '^[0-9a-zA-Z,._-\/]*$' &> /dev/null
		then
			echo "Please use letters, numbers, spaces and special characters ,._-/"
			continue
		fi
		
		x="$(basename ${home})"
		if test "${x}" != "${login}"
		then
			echo "Warning: you are about to use different login name and home directory."
		fi
		
		x="$(dirname ${home})"
		if ! test -d "${x}"
		then
			echo "Directory ${x} does not exist."
			echo "If you still want to use it, please make it manually."
			continue
		fi
		
		def_home_dir="${x}"
		return
	done
}

read_shell () {
	local x

	echo
	while :
	do
		echo -n "Shell: [${def_shell}] "
		read shell
		
		if test -z "${shell}"
		then
			shell="${def_shell}"
			echo "Using ${shell}"
		fi
		
		for x in $(cat /etc/shells)
		do
			if test "${x}" = "${shell}"
			then
				def_shell="${shell}"
				return
			fi
		done

		echo "Possible shells are:"
		cat /etc/shells
	done
}

read_group () {
	echo
	while :
	do
		echo -n "Group: [${def_group}] "
		read group
		
		if test -z "${group}"
		then
			group="${def_group}"
			echo "Using ${group}"
		fi
		
		if test "${group}" = '?'
		then
			less /etc/group
			echo
			continue
		fi

		if check_group "${group}"
		then
			echo "Group ${group} does not exist."
			continue
		fi
		
		def_group="${group}"
		return
	done
}

read_other_groups () {
	echo
	while :
	do
		echo -n "Other groups: [${def_og:-none}] "
		read other_groups
		
		if test "${other_groups}" = '?'
		then
			less /etc/group
			echo
			continue
		fi

		if test -z "${other_groups}"
		then
			if test -n "${def_og}"
			then
				other_groups="${def_og}"
				echo "Using ${other_groups}"
			else	
				echo "No other groups"
				return
			fi
		fi
		
		
		if ! check_other_groups "${other_groups}"
		then
			continue
		fi
		
		def_og="${other_groups}"
		return
	done
}

read_uid () {
	echo
	while :
	do
		echo -n "uid: [first free] "
		read uid
			
		if test -z "${uid}"
		then
			echo "Using first free UID."
			return
		fi
		
		if test "${uid}" = '?'
		then
			less /etc/passwd
			echo
			continue
		fi

		if ! expr "${uid}" : '^[0-9]+$' &> /dev/null
		then
			echo "Please use numbers only."
			continue
		fi
		if test "${uid}" -lt "${uid_low}"
		then
			echo "UID must be greater than ${uid_low}"
			continue
		fi
		if test "${uid}" -gt "${uid_high}"
		then
			echo "UID must be smaller than ${uid_high}"
			continue
		fi
		if ! check_uid "${uid}"
		then
			echo "UID ${uid} is already in use"
			continue
		fi
		
		return
	done
}

read_max_valid_days() {
	echo
	while :
	do
		echo -en "Maximum days between password changes: [${def_pwd_max}] "
		read max_days
		
		if test -z "${max_days}"
		then
			max_days="${def_pwd_max}"
			echo "Using ${max_days}"
			return
		fi
		
		if ! expr "${max_days}" : '^[0-9]+$' &> /dev/null
		then
			echo "Please use numbers only."
			continue
		fi
		if test "${max_days}" -lt 7
		then
			echo "Warning: you are using a value shorter than a week."
		fi
		
		def_pwd_max="${max_days}"
		return	
	done
}

read_min_valid_days() {
	echo
	while :
	do
		echo -en "Minimum days between password changes: [${def_pwd_min}] "
		read min_days
		
		if test -z "${min_days}"
		then
			min_days="${def_pwd_min}"
			echo "Using ${min_days}"
			return
		fi
		
		if ! expr "${min_days}" : '^[0-9]+$' &> /dev/null
		then
			echo "Please use numbers only."
			continue
		fi
		if test "${min_days}" -gt 7
		then
			echo "Warning: you are using a value longer than a week."
		fi
		
		def_pwd_min="${min_days}"
		return	
	done
}

read_warning_days() {
	echo
	while :
	do
		echo -en "Number of warning days before password expires: [${def_pwd_warn}] "
		read warn_days
		
		if test -z "${warn_days}"
		then
			warn_days="${def_pwd_warn}"
			echo "Using ${warn_days}"
		fi

		if ! expr "${warn_days}" : '^[0-9]+$' &> /dev/null
		then
			echo "Please use numbers only."
			continue
		fi
		if test "${warn_days}" -gt 14
		then
			echo "Warning: you are using a value longer than two week."
		fi
		
		def_pwd_warn="${warn_days}"
		return	
	done
}


read_inactive_days() {
	echo
	while :
	do
		echo -en "Number of usable days after expiration: [${def_pwd_iact}] "
		read iact_days
		
		if test -z "${iact_days}"
		then
			iact_days="${def_pwd_iact}"
			echo "Using ${iact_days}"
			return
		fi
		if ! expr "${iact_days}" : '^[0-9]+$' &> /dev/null
		then
			echo "Please use numbers only."
			continue
		fi
		if test "${iact_days}" -gt 14
		then
			echo "Warning: you are using a value that is more than two weeks."
		fi
		
		def_pwd_iact="${iact_days}"
		return	
	done
}

read_expire_date() {
	local ans
	
	echo
	while :
	do
		echo -en "Expire date of this account (mm/dd/yy): [${def_expire:-never}] "
		read ans
		
		if test -z "${ans}"
		then
			if test -z "${def_expire}"
			then
				ans="never"
			else
				ans="${def_expire}"
				echo "Using ${def_expire}"
			fi
		fi
		
		if test "${ans}" = "never"
		then
			echo "Account will never expire."
			def_expire=""
			expire=""
			return
		fi

		if ! expr "${ans}" : '^[0-9][0-9]/[0-9][0-9]/[0-9][0-9]$' &> /dev/null
		then
			echo "Please use format mm/dd/yy"
			continue
		fi
		
		if ! expire_date="$(date -d ${ans} '+%A, %B %d %Y')"
		then
			continue
		fi
		
		def_expire="${expire}"
		return	
	done
}

read_passwd_yn() {
	echo -en "\nDo you want to set password [Y/n] ? "
	if read_yn 0
	then
		set_pwd="YES"
	else
		set_pwd=""
	fi
}


print_values() {

clear
cat << EOM

Login:        ${S}${login}${E}
Group:        ${S}${group}${E}
Other groups: ${S}${other_groups:-[none]}${E}

Real Name:    ${S}${name}${E}

uid:          ${S}${uid:-[first free]}${E}
home:         ${S}${home}${E}
shell:        ${S}${shell}${E}

Account expiration date:                   ${S}${expire_date:-never}${E}
Minimum days between password changes:     ${S}${min_days}${E}
Maximum days between password changes:     ${S}${max_days}${E}
Number of usable days after expiration:    ${S}${iact_days}${E}
Number of warning days before expiration:  ${S}${warn_days}${E}

${S}${set_pwd:+Set password for this account.}${E}

EOM
}

set_user() {
	if ! useradd \
		-c "${name}" \
		-d "${home}" \
		-g "${group}" \
		-s "${shell}" \
		${expire:+-e ${expire}} \
		${uid:+-u ${uid}} \
		${other_groups:+-G ${other_groups}} \
		${login}
	then
		echo "Error ($?) in useradd...exiting..."
		exit 1
	fi
}

set_aging() {
	if ! passwd \
		-x ${max_days} \
		-n ${min_days} \
		-w ${warn_days} \
		-i ${iact_days} \
		${login}
	then
		echo "Error ($?) in setting password aging...exiting..." 
		exit 1
	fi
}

set_password() {
	if test -n "${set_pwd}"
	then
		echo
		passwd ${login}
		echo
	fi
}	

set_system() {
	if test -d "${home}"
	then
		echo "Directory ${home} already exists."
		echo "Skeleton files not copied."
		return
	fi
	
	echo -n "Copying skeleton files..."
	( 
	  mkdir ${home}
	  cd ${skel} && cp -af . ${home}
	  chmod ${def_mode} ${home}
	  chown -R ${login}:${group} ${home}
	)
	echo "done."

	## Add your own stuff here:
	echo -n "Setting up other files..."
	(
	  mailbox="/var/spool/mail/${login}"
	  touch ${mailbox}
	  chown "${login}:mail" ${mailbox}
	  chmod 600 ${mailbox}
	)
	echo "done."
}


read_values() {
	clear
	echo -e "\nPlease answer the following questions about the new user to be added."
	
	while :
	do
		read_login
		read_name
		read_group
		read_other_groups
		read_home
		read_shell
		read_uid
		read_expire_date
		read_max_valid_days
		read_min_valid_days
		read_warning_days
		read_inactive_days
		read_passwd_yn

		print_values
		
		echo -n "Is this correct [N/y] ? "
		read_yn 1 && return
	done
}


main() {
	check_root
	read_values
	set_user
	set_aging
	set_system
	set_password
}


## Run it 8-)
main

# End.
