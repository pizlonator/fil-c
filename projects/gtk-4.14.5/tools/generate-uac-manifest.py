#!/usr/bin/env python3
#
# Copyright © 2021 Chun-wei Fan.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Original author: Chun-wei Fan <fanc999@yahoo.com.tw>

"""
This script generates a Windows manifest file and optionally a resource file to
determine whether a specified program requires UAC elevation
"""

import os
import argparse

DOMAIN_NAME = "gnome"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-p", "--package", required=True, help="package name of the executable"
    )
    parser.add_argument("-n", "--name", required=True, help="name of executable")
    parser.add_argument(
        "--pkg-version", required=True, dest="version", help="version of package"
    )
    parser.add_argument(
        "--require-admin",
        action="store_true",
        dest="admin",
        default=False,
        help="require admin access to application",
    )
    parser.add_argument(
        "--input-resource-file",
        dest="resource",
        default=None,
        help="existing .rc file to embed UAC manifest (do not generate a new .rc file), must have included windows.h in it",
    )
    parser.add_argument(
        "--output-dir",
        dest="outdir",
        default=None,
        help="directory to output resulting files",
    )
    args = parser.parse_args()

    if args.resource is not None:
        if not os.path.isfile(args.resource):
            raise FileNotFoundError(
                "Specified resource file '%s' does not exist" % args.resource
            )

    generate_manifest(args.package, args.name, args.version, args.admin, args.outdir)
    write_rc_file(args.name, args.resource, args.outdir)


def generate_manifest(package, name, version, admin, outdir):
    if version.count(".") == 0:
        manifest_package_version = version + ".0.0.0"
    elif version.count(".") == 1:
        manifest_package_version = version + ".0.0"
    elif version.count(".") == 2:
        manifest_package_version = version + ".0"
    elif version.count(".") == 3:
        manifest_package_version = version
    else:
        parts = version.split(".")
        manifest_package_version = ""
        for x in (0, 1, 2, 3):
            if x == 0:
                manifest_package_version += parts[x]
            else:
                manifest_package_version += "." + parts[x]

    if outdir is not None:
        output_file_base_name = os.path.join(outdir, name)
    else:
        output_file_base_name = name

    outfile = open(output_file_base_name + ".exe.manifest", "w+")
    outfile.write("<?xml version='1.0' encoding='UTF-8' standalone='yes'?>\n")
    outfile.write(
        "<assembly xmlns='urn:schemas-microsoft-com:asm.v1' manifestVersion='1.0'>\n"
    )
    outfile.write("  <assemblyIdentity version='%s'\n" % manifest_package_version)
    outfile.write("    processorArchitecture='*'\n")
    outfile.write("    name='%s.%s.%s.exe'\n" % (DOMAIN_NAME, package, name))
    outfile.write("    type='win32' />\n")
    outfile.write("  <trustInfo xmlns='urn:schemas-microsoft-com:asm.v3'>\n")
    outfile.write("    <security>\n")
    outfile.write("      <requestedPrivileges>\n")
    outfile.write("        <requestedExecutionLevel\n")

    if admin:
        outfile.write("          level='requireAdministrator'\n")
    else:
        outfile.write("          level='asInvoker'\n")

    outfile.write("          uiAccess='false' />\n")
    outfile.write("      </requestedPrivileges>\n")
    outfile.write("    </security>\n")
    outfile.write("  </trustInfo>\n")
    outfile.write("</assembly>\n")
    outfile.close()


def write_rc_file(name, resource, outdir):
    if outdir is not None:
        output_file_base_name = os.path.join(outdir, name)
    else:
        output_file_base_name = name

    if resource is None:
        outfile = open(output_file_base_name + ".rc", "w+")
        outfile.write("#define WIN32_LEAN_AND_MEAN\n")
        outfile.write("#include <windows.h>\n")
    else:
        if resource != output_file_base_name + ".rc":
            outfile = open(output_file_base_name + ".rc", "w+")
        else:
            outfile = open(output_file_base_name + ".final.rc", "w+")
        srcfile = open(resource, "r")
        outfile.write(srcfile.read())
        srcfile.close()

    outfile.write("\n")
    outfile.write(
        'CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST "%s.exe.manifest"' % name
    )
    outfile.close()


if __name__ == "__main__":
    main()
