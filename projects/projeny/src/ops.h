/*
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <string>
#include <vector>

// Resolved locations for "<pdir>/<archive>", "<pdir>/<name>/", etc.
// `projeny_arg` is the .projeny path exactly as given on the command line
// (relative paths resolve against the caller's CWD).
struct Ctx {
    std::string projeny_arg;
    std::string pdir;       // directory containing the .projeny file
    std::string statusfile; // dot-prefixed ".<projeny path>.status" (a
                            // legacy undotted "<projeny path>.status" is
                            // renamed into place on first use)
};

Ctx resolve_ctx(const std::string& projeny_arg);

int cmd_setup(const std::string& projeny_arg);
int cmd_commit(const std::string& projeny_arg);
int cmd_add(const std::string& projeny_arg, const std::string& path);
int cmd_rm(const std::string& projeny_arg, const std::string& path);
int cmd_mv(const std::string& projeny_arg, const std::string& src,
           const std::string& dst);
int cmd_resolve(const std::string& projeny_arg, const std::string& path);
int cmd_rebase(const std::string& projeny_arg, const std::string& new_tarball);
int cmd_status(const std::string& projeny_arg);
int cmd_diff(const std::string& dir, const std::string& other_dir);
int cmd_patch(const std::string& dir, const std::string& patch_file);
int cmd_package(const std::string& projeny_arg, const std::string& output);
int cmd_extract(const std::string& projeny_arg, const std::string& dest_dir);
int cmd_help(const std::string& arg0);
int cmd_help_topic(const std::string& arg0, const std::string& topic);
