## Copyright (C) 2018-2021 Free Software Foundation, Inc.
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <https://www.gnu.org/licenses/>.

cdir = $(docdir)/%D%
dist_c_DATA = %D%/README.md

include %D%/bistromathic/local.mk
include %D%/calc/local.mk
include %D%/glr/local.mk
include %D%/lexcalc/local.mk
include %D%/mfcalc/local.mk
include %D%/pushcalc/local.mk
include %D%/reccalc/local.mk
include %D%/rpcalc/local.mk
