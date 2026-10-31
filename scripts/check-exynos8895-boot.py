#!/usr/bin/env python3
"""Check the raw kernel and Samsung DTBH in a legacy Android boot image.

This is a packaging check, not a device boot test. Run against the repacked
boot.img before flashing; it does not modify the input.
"""
import argparse
import json
from pathlib import Path
import struct


def require(condition, message):
    if not condition:
        raise ValueError(message)


def check_image(data):
    require(not data.startswith(b"\x1f\x8b"),
            "Compressed kernel: repack with arch/arm64/boot/Image, not Image.gz")
    require(len(data) >= 64 and data[56:60] == b"ARM\x64",
            "Kernel is not a raw ARM64 Image")
    offset, memory_size, flags = struct.unpack_from("<QQQ", data, 8)
    require(offset == 0x80000, "Unexpected Samsung kernel text offset")
    require(memory_size >= len(data), "Image header does not cover the kernel payload")
    require(flags & 1 == 0, "Big-endian kernel is not supported by this target")
    return {"payload_bytes": len(data), "memory_bytes": memory_size,
            "text_offset": hex(offset), "flags": hex(flags)}


def check_dtbh(data):
    require(len(data) >= 12 and data[:4] == b"DTBH", "Missing Samsung DTBH table")
    version, count = struct.unpack_from("<II", data, 4)
    require(version == 2 and 0 < count <= 100, "Invalid DTBH version/count")
    table_end = 12 + count * 32
    require(table_end <= len(data), "Truncated DTBH table")
    entries = []
    for index in range(count):
        chip, platform, subtype, first, last, offset, size, _ = struct.unpack_from(
            "<8I", data, 12 + index * 32)
        require(table_end <= offset and size >= 40 and offset + size <= len(data),
                "DTBH entry outside payload")
        require(data[offset:offset + 4] == b"\xd0\x0d\xfe\xed", "Invalid DTB magic")
        total = struct.unpack_from(">I", data, offset + 4)[0]
        require(40 <= total <= size, "Invalid DTB size")
        entries.append({"chip": hex(chip), "platform": hex(platform),
                        "subtype": hex(subtype), "revision": [first, last]})
    return entries


def check_boot(data):
    require(len(data) >= 608 and data[:8] == b"ANDROID!", "Missing Android boot header")
    ks, ka, rs, ra, ss, sa, tags, page, ds, os_version = struct.unpack_from("<10I", data, 8)
    require(page in (2048, 4096, 8192, 16384), "Unsupported boot page size")
    require(ds > 2,
            "Expected Samsung legacy v0 header with dt_size; newer header unsupported")
    def aligned(size):
        return (size + page - 1) // page * page
    dt_start = page + aligned(ks) + aligned(rs) + aligned(ss)
    require(ks > 0 and dt_start + ds <= len(data), "Truncated boot image")
    kernel = check_image(data[page:page + ks])
    entries = check_dtbh(data[dt_start:dt_start + ds])
    return {"kernel": kernel, "dtbh": entries, "header_addresses": {
        "kernel": hex(ka), "ramdisk": hex(ra), "second": hex(sa), "tags": hex(tags)},
        "page_size": page, "os_version_field": hex(os_version),
        "note": "Samsung may override header load addresses; runtime placement is not verified"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="Repacked boot.img, or raw Image with --dtb")
    parser.add_argument("--dtb", type=Path, help="Check raw Image with a separate dtb_*.img")
    args = parser.parse_args()
    try:
        data = args.image.read_bytes()
        if args.dtb:
            result = {"kernel": check_image(data), "dtbh": check_dtbh(args.dtb.read_bytes())}
        else:
            result = check_boot(data)
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, "Boot packaging check failed: %s\n" % error)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
