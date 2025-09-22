/*
 * libproc2 - Library to read proc filesystem
 *
 * Copyright © 2016-2023 Jim Warner <james.warner@comcast.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define STRINGIFY_ARG(a) #a
#define STRINGIFY(a) STRINGIFY_ARG(a)


// --- DISKSTATS ------------------------------------------
#if defined(PROCPS_DISKSTATS_H) && !defined(PROCPS_DISKSTATS_H_DEBUG)
#define PROCPS_DISKSTATS_H_DEBUG

struct diskstats_result *xtra_diskstats_get (
    struct diskstats_info *info,
    const char *name,
    enum diskstats_item actual_enum,
    const char *typestr,
    const char *file,
    int lineno);

# undef DISKSTATS_GET
#define DISKSTATS_GET( info, name, actual_enum, type ) ( { \
    struct diskstats_result *r; \
    r = xtra_diskstats_get(info, name, actual_enum , STRINGIFY(type), __FILE__, __LINE__); \
    r ? r->result . type : 0; } )

struct diskstats_result *xtra_diskstats_val (
    int relative_enum,
    const char *typestr,
    const struct diskstats_stack *stack,
    struct diskstats_info *info,
    const char *file,
    int lineno);

# undef DISKSTATS_VAL
#define DISKSTATS_VAL( relative_enum, type, stack, info ) ( { \
    struct diskstats_result *r; \
    r = xtra_diskstats_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .


// --- MEMINFO --------------------------------------------
#if defined(PROCPS_MEMINFO_H) && !defined(PROCPS_MEMINFO_H_DEBUG)
#define PROCPS_MEMINFO_H_DEBUG

struct meminfo_result *xtra_meminfo_get (
    struct meminfo_info *info,
    enum meminfo_item actual_enum,
    const char *typestr,
    const char *file,
    int lineno);

# undef MEMINFO_GET
#define MEMINFO_GET( info, actual_enum, type ) ( { \
    struct meminfo_result *r; \
    r = xtra_meminfo_get(info, actual_enum , STRINGIFY(type), __FILE__, __LINE__); \
    r ? r->result . type : 0; } )

struct meminfo_result *xtra_meminfo_val (
    int relative_enum,
    const char *typestr,
    const struct meminfo_stack *stack,
    struct meminfo_info *info,
    const char *file,
    int lineno);

# undef MEMINFO_VAL
#define MEMINFO_VAL( relative_enum, type, stack, info ) ( { \
    struct meminfo_result *r; \
    r = xtra_meminfo_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .


// --- PIDS -----------------------------------------------
#if defined(PROCPS_PIDS_H) && !defined(PROCPS_PIDS_H_DEBUG)
#define PROCPS_PIDS_H_DEBUG

struct pids_result *xtra_pids_val (
    int relative_enum,
    const char *typestr,
    const struct pids_stack *stack,
    struct pids_info *info,
    const char *file,
    int lineno);

# undef PIDS_VAL
#define PIDS_VAL( relative_enum, type, stack, info ) ( { \
    struct pids_result *r; \
    r = xtra_pids_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .


// --- SLABINFO -------------------------------------------
#if defined(PROCPS_SLABINFO_H) && !defined(PROCPS_SLABINFO_H_DEBUG)
#define PROCPS_SLABINFO_H_DEBUG

struct slabinfo_result *xtra_slabinfo_get (
    struct slabinfo_info *info,
    enum slabinfo_item actual_enum,
    const char *typestr,
    const char *file,
    int lineno);

# undef SLABINFO_GET
#define SLABINFO_GET( info, actual_enum, type ) ( { \
    struct slabinfo_result *r; \
    r = xtra_slabinfo_get(info, actual_enum , STRINGIFY(type), __FILE__, __LINE__); \
    r ? r->result . type : 0; } )

struct slabinfo_result *xtra_slabinfo_val (
    int relative_enum,
    const char *typestr,
    const struct slabinfo_stack *stack,
    struct slabinfo_info *info,
    const char *file,
    int lineno);

# undef SLABINFO_VAL
#define SLABINFO_VAL( relative_enum, type, stack, info ) ( { \
    struct slabinfo_result *r; \
    r = xtra_slabinfo_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .


// --- STAT -----------------------------------------------
#if defined(PROCPS_STAT_H) && !defined(PROCPS_STAT_H_DEBUG)
#define PROCPS_STAT_H_DEBUG

struct stat_result *xtra_stat_get (
    struct stat_info *info,
    enum stat_item actual_enum,
    const char *typestr,
    const char *file,
    int lineno);

# undef STAT_GET
#define STAT_GET( info, actual_enum, type ) ( { \
    struct stat_result *r; \
    r = xtra_stat_get(info, actual_enum , STRINGIFY(type), __FILE__, __LINE__); \
    r ? r->result . type : 0; } )

struct stat_result *xtra_stat_val (
    int relative_enum,
    const char *typestr,
    const struct stat_stack *stack,
    struct stat_info *info,
    const char *file,
    int lineno);

# undef STAT_VAL
#define STAT_VAL( relative_enum, type, stack, info ) ( { \
    struct stat_result *r; \
    r = xtra_stat_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .


// --- VMSTAT ---------------------------------------------
#if defined(PROCPS_VMSTAT_H) && !defined(PROCPS_VMSTAT_H_DEBUG)
#define PROCPS_VMSTAT_H_DEBUG

struct vmstat_result *xtra_vmstat_get (
    struct vmstat_info *info,
    enum vmstat_item actual_enum,
    const char *typestr,
    const char *file,
    int lineno);

# undef VMSTAT_GET
#define VMSTAT_GET( info, actual_enum, type ) ( { \
    struct vmstat_result *r; \
    r = xtra_vmstat_get(info, actual_enum , STRINGIFY(type), __FILE__, __LINE__); \
    r ? r->result . type : 0; } )

struct vmstat_result *xtra_vmstat_val (
    int relative_enum,
    const char *typestr,
    const struct vmstat_stack *stack,
    struct vmstat_info *info,
    const char *file,
    int lineno);

# undef VMSTAT_VAL
#define VMSTAT_VAL( relative_enum, type, stack, info ) ( { \
    struct vmstat_result *r; \
    r = xtra_vmstat_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .
