## Process this file with automake to produce Makefile.in

## Copyright (C) 2010-2017 Colin Watson.
##
## This file is part of libpipeline.
##
## libpipeline is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or (at
## your option) any later version.
##
## libpipeline is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with libpipeline; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
## USA

dist_man3_MANS = libpipeline.3

FUNCTIONS = \
	pipecmd_new \
	pipecmd_new_argv \
	pipecmd_new_args \
	pipecmd_new_argstr \
	pipecmd_new_function \
	pipecmd_new_sequencev \
	pipecmd_new_sequence \
	pipecmd_new_passthrough \
	pipecmd_dup \
	pipecmd_arg \
	pipecmd_argf \
	pipecmd_argv \
	pipecmd_args \
	pipecmd_argstr \
	pipecmd_get_nargs \
	pipecmd_nice \
	pipecmd_discard_err \
	pipecmd_chdir \
	pipecmd_fchdir \
	pipecmd_setenv \
	pipecmd_unsetenv \
	pipecmd_clearenv \
	pipecmd_pre_exec \
	pipecmd_sequence_command \
	pipecmd_dump \
	pipecmd_tostring \
	pipecmd_exec \
	pipecmd_free \
	pipeline_new \
	pipeline_new_commandv \
	pipeline_new_commands \
	pipeline_new_command_argv \
	pipeline_new_command_args \
	pipeline_join \
	pipeline_connect \
	pipeline_command \
	pipeline_command_argv \
	pipeline_command_args \
	pipeline_command_argstr \
	pipeline_commandv \
	pipeline_commands \
	pipeline_want_in \
	pipeline_want_out \
	pipeline_want_infile \
	pipeline_want_outfile \
	pipeline_ignore_signals \
	pipeline_get_ncommands \
	pipeline_get_command \
	pipeline_set_command \
	pipeline_get_pid \
	pipeline_get_infile \
	pipeline_get_outfile \
	pipeline_dump \
	pipeline_tostring \
	pipeline_free \
	pipeline_install_post_fork \
	pipeline_start \
	pipeline_wait_all \
	pipeline_wait \
	pipeline_run \
	pipeline_pump \
	pipeline_read \
	pipeline_peek \
	pipeline_peek_size \
	pipeline_peek_skip \
	pipeline_readline \
	pipeline_peekline

install-data-hook:
	set -e; cd "$(DESTDIR)$(man3dir)"; for function in $(FUNCTIONS); do \
		rm -f $$function.3; \
		$(LN_S) libpipeline.3 $$function.3; \
	done

uninstall-hook:
	set -e; cd "$(DESTDIR)$(man3dir)"; for function in $(FUNCTIONS); do \
		rm -f $$function.3; \
	done
