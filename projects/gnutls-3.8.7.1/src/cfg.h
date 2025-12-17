/*
 * Copyright (C) 2021 Daiki Ueno
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#ifndef CFG_H_
#define CFG_H_ 1

typedef struct cfg_option_st {
	char *name;
	char *value;
} *cfg_option_t;

cfg_option_t cfg_load(const char *filename);
void cfg_free(cfg_option_t options);
cfg_option_t cfg_next(const cfg_option_t options, const char *name);

#endif /* CFG_H_ */
