#!/usr/bin/env python3
"""Build a kernel-mode driver with the installed WDK kit and MSVC.

No Visual Studio driver toolset is required: the script sets INCLUDE/LIB/PATH
itself and invokes cl.exe /kernel and link.exe /DRIVER.

    python build.py --source driver/probe/DriverEntry.c --name CableAudioProbe
    python build.py --project driver/CableAudio --name CableAudio --sign
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys

KITS = r"C:\Program Files (x86)\Windows Kits\10"

VS_ROOTS = [
    r"C:\Program Files\Microsoft Visual Studio\2022\Community",
    r"C:\Program Files\Microsoft Visual Studio\2022\Professional",
    r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
    r"C:\Program Files\Microsoft Visual Studio\2022\BuildTools",
    r"C:\Program Files\Microsoft Visual Studio\18\Community",
    r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools",
]

DRIVER_DEFINES = [
    "_WIN32",
    "UNICODE",
    "_UNICODE",
    "PC_IMPLEMENTATION",
    "DEBUG_LEVEL=0",
    "KMDF_VERSION_MAJOR=1",
    "KMDF_VERSION_MINOR=33",
    "_NEW_DELETE_OPERATORS_",
    "_USE_WAVERT_",
    "_USE_IPortClsRuntimePower",
]

# The simple audio sample links portcls plus the static UNKNOWN support and
# kernel CRT helpers.
DRIVER_LIBS = [
    "portcls.lib",
    "stdunk.lib",
    "libcntpr.lib",
    "wdfdriverentry.lib",
    "wdfldr.lib",
    "ntoskrnl.lib",
    "bufferoverflowfastfailk.lib",
    "hal.lib",
]


def find_wdk() -> str:
    include = os.path.join(KITS, "Include")
    if not os.path.isdir(include):
        sys.exit(f"WDK not found: {include}")
    versions = []
    for name in os.listdir(include):
        parts = name.split(".")
        if len(parts) == 4 and all(p.isdigit() for p in parts):
            versions.append(name)
    for version in sorted(versions, key=lambda v: tuple(int(p) for p in v.split(".")), reverse=True):
        if os.path.isfile(os.path.join(include, version, "km", "ntddk.h")) and os.path.isfile(
            os.path.join(KITS, "Lib", version, "km", "x64", "ntoskrnl.lib")
        ):
            return version
    sys.exit("No WDK with km headers and x64 libs found.")


def find_msvc() -> tuple[str, str]:
    for root in VS_ROOTS:
        tools = os.path.join(root, "VC", "Tools", "MSVC")
        if not os.path.isdir(tools):
            continue
        for version in sorted(os.listdir(tools), reverse=True):
            candidate = os.path.join(tools, version)
            if os.path.isdir(os.path.join(candidate, "include")) and os.path.isdir(
                os.path.join(candidate, "lib", "x64")
            ):
                return root, candidate
    sys.exit("No MSVC toolset with include/ and lib/x64 found.")


def collect_sources(root: str) -> list[str]:
    found = []
    for dirpath, _dirnames, filenames in os.walk(root):
        for filename in filenames:
            if filename.lower().endswith((".cpp", ".c")):
                found.append(os.path.join(dirpath, filename))
    return sorted(found)


def collect_includes(root: str) -> list[str]:
    dirs = set()
    for dirpath, _dirnames, filenames in os.walk(root):
        if any(name.lower().endswith(".h") for name in filenames):
            dirs.add(dirpath)
    return sorted(dirs)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", default="")
    parser.add_argument("--project", default="")
    parser.add_argument("--name", default="CableAudio")
    parser.add_argument("--entry", default="DriverEntry")
    parser.add_argument("--outdir", default="")
    parser.add_argument("--define", action="append", default=[])
    parser.add_argument("--sign", action="store_true")
    parser.add_argument("--cert", default="")
    args = parser.parse_args()

    if bool(args.source) == bool(args.project):
        sys.exit("Pass exactly one of --source or --project.")

    project = ""
    if args.project:
        project = os.path.abspath(args.project)
        sources = collect_sources(project)
        includes = collect_includes(project)
        outdir = os.path.abspath(args.outdir) if args.outdir else os.path.join(project, "out")
    else:
        source = os.path.abspath(args.source)
        if not os.path.isfile(source):
            sys.exit(f"Source not found: {source}")
        sources = [source]
        includes = []
        outdir = os.path.abspath(args.outdir) if args.outdir else os.path.join(
            os.path.dirname(source), "out"
        )
    if not sources:
        sys.exit("No sources found.")

    os.makedirs(outdir, exist_ok=True)

    wdk_version = find_wdk()
    vs_root, msvc = find_msvc()
    print(f"WDK  : {wdk_version}")
    print(f"VS   : {vs_root}")
    print(f"Files: {len(sources)}")

    km_inc = os.path.join(KITS, "Include", wdk_version, "km")
    shared_inc = os.path.join(KITS, "Include", wdk_version, "shared")
    ucrt_inc = os.path.join(KITS, "Include", wdk_version, "ucrt")
    km_lib = os.path.join(KITS, "Lib", wdk_version, "km", "x64")
    wdf_inc = os.path.join(KITS, "Include", "wdf", "kmdf", "1.33")
    wdf_lib = os.path.join(KITS, "Lib", "wdf", "kmdf", "x64", "1.33")
    msvc_inc = os.path.join(msvc, "include")
    msvc_lib = os.path.join(msvc, "lib", "x64")
    msvc_bin = os.path.join(msvc, "bin", "Hostx64", "x64")

    sysfile = os.path.join(outdir, args.name + ".sys")
    cmd_path = os.path.join(outdir, "build.cmd")

    includes_all = [km_inc, shared_inc, ucrt_inc, msvc_inc, wdf_inc] + includes
    define_flags = " ".join(
        ["/D_AMD64_", "/DAMD64", "/D_WIN64", "/DWIN32=100",
         "/D_WIN32_WINNT=0x0A00", "/DNTDDI_VERSION=0x0A00000C"]
        + [f'/D{d.replace(chr(34), "")}' for d in (DRIVER_DEFINES + args.define)]
    )
    include_flags = " ".join(f'/I"{path}"' for path in includes_all)

    objects = []
    compile_lines = []
    for source in sources:
        rel = os.path.relpath(source, project if args.project else os.path.dirname(source))
        obj = os.path.join(outdir, rel.replace("\\", "_").replace("/", "_") + ".obj")
        objects.append(obj)
        compile_lines.append(
            f'cl /nologo /c /kernel /W4 /WX- /Od /Z7 "{source}" {define_flags} {include_flags} /Fo"{obj}"'
        )
        compile_lines.append("if errorlevel 1 exit /b 1")

    lib_flags = " ".join(f'/LIBPATH:"{path}"' for path in (km_lib, wdf_lib))
    quoted_objects = " ".join(f'"{o}"' for o in objects)
    link_line = (
        f"link /nologo /DRIVER /MACHINE:X64 /SUBSYSTEM:NATIVE /ENTRY:{args.entry} "
        f"/NODEFAULTLIB:LIBCMT.lib /NODEFAULTLIB:LIBCMTD.lib /NODEFAULTLIB:OLDNAMES.lib "
        f'/OUT:"{sysfile}" {quoted_objects} {lib_flags} {" ".join(DRIVER_LIBS)}'
    )
    include_env = ";".join(includes_all)
    lib_env = ";".join([km_lib, wdf_lib])
    lines = [
        "@echo off",
        f"set INCLUDE={include_env}",
        f"set LIB={lib_env}",
        f"set PATH={msvc_bin};%PATH%",
        *compile_lines,
        link_line,
        "if errorlevel 1 exit /b 1",
        "exit /b 0",
    ]
    with open(cmd_path, "w", encoding="ascii", newline="\r\n") as handle:
        handle.write("\n".join(lines) + "\n")

    print(f"Build: {cmd_path}")
    result = subprocess.run(["cmd", "/c", cmd_path], cwd=outdir)
    if result.returncode != 0:
        return result.returncode
    if not os.path.isfile(sysfile):
        print("No .sys produced", file=sys.stderr)
        return 1
    print(f"Built: {sysfile} ({os.path.getsize(sysfile) / 1024:.1f} KB)")

    if args.sign:
        cert = args.cert or os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "tools", "signing", "test-signing.pfx",
        )
        if not os.path.isfile(cert):
            sys.exit(f"Certificate not found: {cert} (run tools/make-test-cert.ps1)")
        signtool = os.path.join(KITS, "bin", wdk_version, "x64", "signtool.exe")
        signed = subprocess.run(
            [signtool, "sign", "/f", cert, "/p", "test", "/fd", "SHA256", "/a", sysfile]
        )
        if signed.returncode != 0:
            return signed.returncode
        print(f"Signed: {sysfile}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
