#!/usr/bin/env python3
"""Build a kernel-mode driver with the installed WDK kit and MSVC.

No Visual Studio driver toolset is required: the script sets INCLUDE/LIB/PATH
itself and invokes cl.exe /kernel and link.exe /DRIVER.

    python build.py driver/probe/DriverEntry.c --name CableAudioProbe
    python build.py driver/probe/DriverEntry.c --name CableAudioProbe --sign
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source")
    parser.add_argument("--name", default="CableAudio")
    parser.add_argument("--outdir", default="")
    parser.add_argument("--sign", action="store_true")
    parser.add_argument("--cert", default="")
    args = parser.parse_args()

    source = os.path.abspath(args.source)
    if not os.path.isfile(source):
        sys.exit(f"Source not found: {source}")
    outdir = os.path.abspath(args.outdir) if args.outdir else os.path.join(
        os.path.dirname(source), "out"
    )
    os.makedirs(outdir, exist_ok=True)

    wdk_version = find_wdk()
    vs_root, msvc = find_msvc()
    print(f"WDK  : {wdk_version}")
    print(f"VS   : {vs_root}")

    km_inc = os.path.join(KITS, "Include", wdk_version, "km")
    shared_inc = os.path.join(KITS, "Include", wdk_version, "shared")
    ucrt_inc = os.path.join(KITS, "Include", wdk_version, "ucrt")
    km_lib = os.path.join(KITS, "Lib", wdk_version, "km", "x64")
    msvc_inc = os.path.join(msvc, "include")
    msvc_lib = os.path.join(msvc, "lib", "x64")
    msvc_bin = os.path.join(msvc, "bin", "Hostx64", "x64")

    obj = os.path.join(outdir, args.name + ".obj")
    sysfile = os.path.join(outdir, args.name + ".sys")
    cmd_path = os.path.join(outdir, "build.cmd")

    define_flags = " ".join(
        [
            "/D_AMD64_", "/DAMD64", "/D_WIN64", "/DWIN32=100",
            "/D_WIN32_WINNT=0x0A00", "/DNTDDI_VERSION=0x0A00000C",
            "/DDBG=0", "/DUNICODE", "/D_UNICODE",
        ]
    )
    cl_line = (
        f'cl /nologo /c /kernel /W4 /WX- /Od /Z7 "{source}" {define_flags} '
        f'/Fo"{obj}"'
    )
    link_line = (
        f'link /nologo /DRIVER /MACHINE:X64 /SUBSYSTEM:NATIVE /ENTRY:DriverEntry '
        f'/OUT:"{sysfile}" "{obj}" /LIBPATH:"{km_lib}" /LIBPATH:"{msvc_lib}" '
        f'ntoskrnl.lib bufferoverflowfastfailk.lib'
    )
    include = ";".join([km_inc, shared_inc, ucrt_inc, msvc_inc])
    lib = ";".join([km_lib, msvc_lib])

    lines = [
        "@echo off",
        f"set INCLUDE={include}",
        f"set LIB={lib}",
        f"set PATH={msvc_bin};%PATH%",
        cl_line,
        "if errorlevel 1 exit /b 1",
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
