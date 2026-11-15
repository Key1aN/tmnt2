#!/usr/bin/env python3
"""Builds TMNT2_Enhanced_Graphics_v1.1.t2mod in the T2 Mod Manager package format.

Format (identical to the packages produced by T2ModManager.Packager pack-composable-exe):
  * ZIP with manifest.json (deflate) and payload/Enhanced_Graphics.t2delta (stored)
  * T2DELTA1 stream made of DATA instructions only (Brotli, <= 1 MiB raw each). The
    manager checks the player's TMNT2.exe against the retail US hash in the header,
    rebuilds the core EXE purely from the payload, then verifies the output SHA-256.
  * No retail EXE or DAT bytes are contained in the package.

Usage:
  pack_t2mod.py CORE_EXE OUTPUT.t2mod                      (release package)
  pack_t2mod.py CORE_EXE OUTPUT.t2mod --test-source FILE   (format test against a synthetic source)
"""
import argparse, hashlib, io, json, struct, sys, zipfile

import brotli

RETAIL_EXE_SIZE = 4796416
RETAIL_EXE_SHA = "b827478461e1687f9404679211fc2d2d47bd7222aeb81a8554bd793575a89564"
RETAIL_DAT_SHA = "cadae3cea3ad60a1f61acf4984f47adb15efccc880fe82c254b332196caa04b2"
CHUNK = 1024 * 1024

PACKAGE_ID = "Enhanced_Graphics"
FEATURE = "enhanced-graphics"
VERSION = "1.1.0"
DISPLAY_NAME = "Enhanced Graphics v1.1 (cel lighting, contact shadows, stylized water)"
DESCRIPTION = (
    "v1.1: controller-friendly REMASTER tab in Debug Tools (F4) to switch and tune every setting live and save "
    "it; water detection no longer picks static floors; flat toon ramps left as shipped. "
    "Modern remaster of the 3D look that keeps the 2003 cartoon style. Toon lighting: remapped shading "
    "gradients with a crisper light/shadow split, cool stage-tinted shadows and a lifted highlight band; "
    "warm key / cool fill light rig for characters (ink outlines untouched). Shadows: soft contact shadows "
    "with a dark core under the feet that fade and widen with height. Water: animated stylized overlay on "
    "detected water polygons with waves, sky reflection, toon specular glints, shallow/deep colour and "
    "shoreline foam; collision, splashes and ripples unchanged. Post: subtle bloom and colour grade "
    "(optional FXAA), HUD excluded. F8 = original/enhanced, "
    "Shift+F8 = Classic+/Enhanced/High/Custom, Ctrl+F8 = reload TMNT2_graphics.ini. Includes the Debug "
    "Tools v1.2.24 / Camera Overhaul v1.0.1 core (its features stay off unless their own packages are "
    "enabled). EXE-only: does not modify TMNT.DAT, LPAC resources or save data."
)
CORE_ID = "TMNT2_Composable_Core_v1_US"
CORE_PRIORITY = 160
CAPABILITIES = ["intro-skip", "msaa", "brutal-difficulties", "widescreen", "ultimate-slashuur",
                "debug-tools-f4", "karai-boss-variant", "combo-assist", FEATURE]


def make_delta(target, src_size, src_sha):
    out = io.BytesIO()
    chunks = [target[i:i + CHUNK] for i in range(0, len(target), CHUNK)]
    out.write(b"T2DELTA1")
    out.write(struct.pack("<I", 1))
    out.write(struct.pack("<qq", src_size, len(target)))
    out.write(bytes.fromhex(src_sha))
    out.write(hashlib.sha256(target).digest())
    out.write(struct.pack("<i", len(chunks)))
    for raw in chunks:
        comp = brotli.compress(raw, quality=11, lgwin=22)
        out.write(struct.pack("<B", 1))
        out.write(struct.pack("<ii", len(raw), len(comp)))
        out.write(comp)
    return out.getvalue()


def apply_delta_check(delta, src_size, src_sha):
    """Independent re-implementation of T2Delta.ApplyAsync validation for DATA-only streams."""
    f = io.BytesIO(delta)
    assert f.read(8) == b"T2DELTA1" and struct.unpack("<I", f.read(4))[0] == 1
    s_len, t_len = struct.unpack("<qq", f.read(16))
    s_hash, t_hash = f.read(32), f.read(32)
    (count,) = struct.unpack("<i", f.read(4))
    assert s_len == src_size and s_hash.hex() == src_sha
    assert 0 < count <= 10_000_000
    out = bytearray()
    for _ in range(count):
        (op,) = struct.unpack("<B", f.read(1))
        assert op == 1, "copy instructions would need the retail source"
        raw_len, comp_len = struct.unpack("<ii", f.read(8))
        assert 0 < raw_len <= CHUNK and 0 < comp_len <= 2 * CHUNK and comp_len <= len(delta) - f.tell()
        data = brotli.decompress(f.read(comp_len))
        assert len(data) == raw_len and len(out) + raw_len <= t_len
        out += data
    assert len(out) == t_len and f.tell() == len(delta)
    assert hashlib.sha256(out).digest() == t_hash
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("core_exe")
    ap.add_argument("output")
    ap.add_argument("--test-source", help="synthetic source file instead of the retail EXE identity (tests only)")
    a = ap.parse_args()

    target = open(a.core_exe, "rb").read()
    assert target[:2] == b"MZ", "not a PE file"
    pe = struct.unpack("<I", target[0x3C:0x40])[0]
    assert target[pe:pe + 4] == b"PE\0\0" and struct.unpack("<H", target[pe + 4:pe + 6])[0] == 0x14C, "not an x86 PE"
    assert b"feature=enhanced-graphics" in target, "core EXE does not contain the enhanced-graphics feature"

    if a.test_source:
        src = open(a.test_source, "rb").read()
        src_size, src_sha = len(src), hashlib.sha256(src).hexdigest()
    else:
        src_size, src_sha = RETAIL_EXE_SIZE, RETAIL_EXE_SHA

    delta = make_delta(target, src_size, src_sha)
    assert apply_delta_check(delta, src_size, src_sha) == target

    payload = "payload/%s.t2delta" % PACKAGE_ID
    manifest = {
        "schemaVersion": 1,
        "id": PACKAGE_ID,
        "displayName": DISPLAY_NAME,
        "version": VERSION,
        "description": DESCRIPTION,
        "target": {
            "game": "TMNT2_Battle_Nexus_PC",
            "region": "US/NA",
            "architecture": "x86",
            "executableSha256": src_sha,
            "archiveSha256": RETAIL_DAT_SHA,
        },
        "resources": [
            {"id": "file:TMNT2.exe", "mode": "exclusive"},
            {"id": "feature:" + FEATURE, "mode": "exclusive"},
        ],
        "dependencies": [],
        "conflicts": [],
        "core": {"id": CORE_ID, "priority": CORE_PRIORITY, "capabilities": CAPABILITIES},
        "featureFlags": [FEATURE],
        "operations": [{
            "type": "apply-t2delta",
            "target": "TMNT2.exe",
            "payload": payload,
            "payloadSha256": hashlib.sha256(delta).hexdigest(),
            "payloadSize": len(delta),
            "expectedInputSha256": src_sha,
            "expectedInputSize": src_size,
            "expectedOutputSha256": hashlib.sha256(target).hexdigest(),
            "expectedOutputSize": len(target),
        }],
    }
    assert len(DESCRIPTION) <= 2000 and len(DISPLAY_NAME) <= 120, (len(DESCRIPTION), len(DISPLAY_NAME))
    mjson = json.dumps(manifest, indent=2, ensure_ascii=False).replace("\n", "\r\n").encode("utf-8")
    with zipfile.ZipFile(a.output, "x") as z:
        z.writestr(zipfile.ZipInfo("manifest.json", (1980, 1, 1, 0, 0, 0)), mjson, compress_type=zipfile.ZIP_DEFLATED)
        z.writestr(zipfile.ZipInfo(payload, (1980, 1, 1, 0, 0, 0)), delta, compress_type=zipfile.ZIP_STORED)

    pkg = open(a.output, "rb").read()
    print(json.dumps({"package": a.output, "packageSha256": hashlib.sha256(pkg).hexdigest(), "packageSize": len(pkg),
                      "payloadSha256": manifest["operations"][0]["payloadSha256"], "payloadSize": len(delta),
                      "coreExeSha256": manifest["operations"][0]["expectedOutputSha256"], "coreExeSize": len(target)}, indent=1))


if __name__ == "__main__":
    main()
