#!/usr/bin/env python3
"""Cross-builds the TMNT2 US Release (Debug Tools ON, d3d9) with MinGW-w64 i686.

This mirrors CMakeLists.txt + cmake/PlatformWin.cmake (same sources, defines and
RenderWare/CRI libraries). It exists because the MSVC toolchain is not available
in every environment; the GitHub Actions MSVC build stays the reference build.
ABI notes: RenderWare and CRI are MSVC-built C libraries, so we keep MSVC struct
layout (-mms-bitfields), assume only 4-byte incoming stack alignment for callbacks
from MSVC code, use SSE2 float math like MSVC's default, and provide the few MSVC
CRT helpers msvcrt.dll does not export (msvc_shims.S / msvc_shims.c).
"""
import concurrent.futures, glob, os, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.environ.get("EG_MINGW_OUT", os.path.join(ROOT, "build", "mingw"))
SHIM = os.environ.get("EG_HEADER_SHIM", "")   # optional dir with case-fixed header links (Linux)
CXX, CC, WINDRES = "i686-w64-mingw32-g++", "i686-w64-mingw32-gcc", "i686-w64-mingw32-windres"
SRC = os.path.join(ROOT, "src")
RW = os.path.join(ROOT, "vendor", "rwsdk37", "win")
IMGUI = os.path.join(ROOT, "vendor", "imgui")

DEFINES = ["NDEBUG", "WIN32", "_WINDOWS", "_MBCS", "TARGET_PC", "TMNT2_RWDRV_D3D9", "TMNT2_SOUND_DSOUND",
           "TMNT2_DEBUG_TOOLS", "TMNT2_IMGUI", "IMGUI_DISABLE_OBSOLETE_FUNCTIONS"]
INCLUDES = [SRC, os.path.join(SRC, "System", "Common", "Sound"), os.path.join(ROOT, "vendor", "cri", "include"),
            os.path.join(RW, "include", "d3d9"), IMGUI, os.path.join(ROOT, "vendor", "fonts")]
ARCH = ["-march=pentium4", "-msse2", "-mfpmath=sse", "-mincoming-stack-boundary=2", "-mms-bitfields",
        "-fno-strict-aliasing", "-fwrapv", "-fno-omit-frame-pointer"]
CXXFLAGS = ["-std=gnu++14", "-O2", "-fno-rtti", "-fms-extensions", "-fpermissive", "-w"] + ARCH
CFLAGS = ["-O2", "-w"] + ARCH
RWLIBS = ["rwcore", "rpcollis", "rphanim", "rplodatm", "rppvs", "rprandom", "rptoon", "rpuvanim", "rpworld", "rt2d",
          "rt2danim", "rtanim", "rtbmp", "rtcharse", "rtdict", "rtfsyst", "rtintsec", "rtpitexd", "rtpng", "rtquat",
          "rpskin", "rpmatfx"]


def glob_r(d, exts=("cpp", "c")):
    out = []
    for e in exts:
        out += glob.glob(os.path.join(d, "**", "*." + e), recursive=True)
    return sorted(out)


def sources():
    game = [f for f in glob_r(os.path.join(SRC, "Game")) if os.sep + os.path.join("Sequence", "Test") + os.sep not in f]
    game.append(os.path.join(SRC, "Game", "Sequence", "Test", "DebugUtils.cpp"))   # OPT_DEBUG_TOOLS keeps this one
    sound = os.path.join(SRC, "System", "Common", "Sound")
    common = [f for f in glob_r(os.path.join(SRC, "System", "Common")) if not f.startswith(sound + os.sep)]
    common += sorted(glob.glob(os.path.join(sound, "*.cpp")) + glob.glob(os.path.join(sound, "*.c")))
    pc = glob_r(os.path.join(SRC, "System", "PC"))
    snd = glob_r(os.path.join(sound, "Backend", "DSound")) + glob_r(os.path.join(sound, "Os", "Win"))
    im = [os.path.join(IMGUI, n) for n in ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp",
                                           os.path.join("backends", "imgui_impl_win32.cpp"))]
    return game + common + pc + snd, im


def obj_for(src):
    rel = os.path.relpath(src, ROOT).replace(os.sep, "_").replace(".", "_")
    return os.path.join(OUT, "obj", rel + ".o")


def compile_one(args):
    src, is_imgui = args
    obj = obj_for(src)
    if os.path.exists(obj) and os.path.getmtime(obj) > os.path.getmtime(src) and not os.environ.get("EG_FORCE"):
        return src, 0, ""
    inc = (["-I" + SHIM] if SHIM else []) + ["-I" + i for i in INCLUDES]
    defs = ["-D" + d for d in DEFINES]
    if src.endswith(".c"):
        cmd = [CC] + CFLAGS + defs + inc + ["-c", src, "-o", obj]
    else:
        pre = [] if is_imgui else ["-include", os.path.join(os.path.dirname(os.path.abspath(__file__)), "mingw_compat.hpp"), "-include", "cstring", "-include", "cstdlib", "-include", "cstdio", "-include", "cmath",
                                   "-include", os.path.join(SRC, "Game", "pch.hpp")]
        cmd = [CXX] + CXXFLAGS + defs + inc + pre + ["-c", src, "-o", obj]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return src, r.returncode, r.stderr


def main():
    os.makedirs(os.path.join(OUT, "obj"), exist_ok=True)
    srcs, im = sources()
    jobs = [(s, False) for s in srcs] + [(s, True) for s in im]
    failed = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        for src, rc, err in ex.map(compile_one, jobs):
            if rc:
                failed.append(src)
                print("FAIL", os.path.relpath(src, ROOT)); print("\n".join(err.splitlines()[:8]))
    if failed:
        sys.exit("%d translation units failed" % len(failed))
    print("compiled %d translation units" % len(jobs))

    here = os.path.dirname(os.path.abspath(__file__))
    shim_objs = []
    for s in ("msvc_shims.S", "msvc_shims.c", "msvc_guids.c"):
        o = os.path.join(OUT, "obj", s + ".o")
        subprocess.check_call([CC] + ARCH + ["-O2", "-c", os.path.join(here, s), "-o", o])
        shim_objs.append(o)
    res = os.path.join(OUT, "obj", "tmnt2_rc.o")
    raw = open(os.path.join(SRC, "System", "PC", "tmnt2.rc"), "rb").read()
    text = raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8", "replace")
    rc8 = os.path.join(OUT, "obj", "tmnt2_utf8.rc")
    open(rc8, "w", encoding="utf-8").write(text.lstrip("\ufeff"))
    subprocess.check_call([WINDRES, "-c", "65001", "-I", SRC, "-I", os.path.join(SRC, "System", "PC"), rc8, "-O", "coff", "-o", res])

    objs = [obj_for(s) for s, _ in jobs] + shim_objs + [res]
    rsp = os.path.join(OUT, "link.rsp")
    open(rsp, "w").write("\n".join('"%s"' % o for o in objs))
    exe = os.path.join(OUT, "TMNT2.exe")

    # Default MinGW linker script plus one rule: drop MSVC SafeSEH tables (.sxdata), which GNU ld
    # would otherwise place at address 0 (the MSVC build links with /SAFESEH:NO as well).
    ldv = subprocess.run(["i686-w64-mingw32-ld", "--verbose"], capture_output=True, text=True).stdout
    script = ldv.split("==================================================")[1]
    script = script.replace("  /DISCARD/ :\n  {", "  /DISCARD/ :\n  {\n    *(.sxdata)", 1)
    lds = os.path.join(OUT, "tmnt2_mingw.ld")
    open(lds, "w").write(script)
    cmd = [CXX, "-o", exe, "@" + rsp, "-Wl,-T," + lds, "-s", "-mwindows", "-static", "-static-libgcc", "-static-libstdc++",
           "-Wl,--image-base,0x400000", "-Wl,--no-insert-timestamp", "-Wl,--disable-dynamicbase", "-Wl,--disable-nxcompat", "-Wl,-Map," + exe[:-4] + ".map",
           "-L" + os.path.join(RW, "lib", "d3d9", "release"), "-L" + os.path.join(ROOT, "vendor", "cri", "lib", "x86"),
           "-Wl,--start-group"] + ["-l:" + l + ".lib" for l in RWLIBS] + \
          ["-l:cri_adxpcx86_dsound8.lib", "-l:cri_mwsfdpcx86.lib", "-Wl,--end-group",
           "-ld3d9", "-ldinput8", "-ldsound", "-lwinmm", "-lksuser", "-ldwmapi", "-lole32", "-luuid",
           "-ldbghelp", "-lgdi32", "-luser32", "-lkernel32", "-limm32", "-lshell32", "-lversion", "-lmsvcrt"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        undef = sorted(set(l.split("undefined reference to `")[1].rstrip("'") for l in r.stderr.splitlines() if "undefined reference to" in l))
        noise = ("corrupt .drectve", "Warning: .drectve")
        errs = [l for l in r.stderr.splitlines() if not any(n in l for n in noise)]
        print("\n".join(errs[-60:]) if not undef else "\n".join(undef))
        sys.exit("link failed (%d undefined symbols)" % len(undef))
    print("linked", exe, os.path.getsize(exe), "bytes")


if __name__ == "__main__":
    main()
