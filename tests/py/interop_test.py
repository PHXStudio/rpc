#!/usr/bin/env python3
"""Cross-language golden check for the InteropFull schema, run against the
generated Python code.

The expected bytes are *the same vector* asserted by
tests/runtime/wire_format_golden_test.cpp and the Go test under tests/go/.
Holding three independently written backends to one hand-derived byte string is
what makes the wire format a shared contract rather than three implementations
that merely happen to agree.

Usage: interop_test.py <generated_py_dir> <runtime_py_dir>
"""

import sys

# Every scalar width, an enum, a string, bytes, a nested struct, an empty
# struct and two arrays, all with non-default values.
GOLDEN = (
    "03 ff ff c0"                  # fmLen=3; 18 fields, all non-default
    " ff"                          # i8_  = -1
    " 02"                          # u8_  = 2
    " fd ff"                       # i16_ = -3
    " 04 00"                       # u16_ = 4
    " fb ff ff ff"                 # i32_ = -5
    " 06 00 00 00"                 # u32_ = 6
    " f9 ff ff ff ff ff ff ff"     # i64_ = -7
    " 08 00 00 00 00 00 00 00"     # u64_ = 8
    " 00 00 c0 3f"                 # f32_ = 1.5
    " 00 00 00 00 00 00 04 40"     # f64_ = 2.5
    " 01"                          # kind_ = IR1   (b_ = True has no payload byte)
    " 02 68 69"                    # text_ = "hi"
    " 02 aa bb"                    # blob_ = {0xAA, 0xBB}
    " 01 c0 09 00 00 00"           # inner_: own mask, x_=9, flag_ is a mask bit
    " 02 01 00 00 00 02 00 00 00"  # ints_ = {1, 2}   (empty_ writes nothing)
    " 02 01 61 02 62 63"           # words_ = {"a", "bc"}
)

# Only i32_ set: the nested and empty structs still hold their mask bits but
# contribute no data; the empty one contributes no mask segment either.
GOLDEN_SPARSE = "03 08 03 00 01 00 00 00 01 00"


class Failure(Exception):
    pass


def to_hex(buf):
    """Join the buffer produced by serialize().

    No normalisation here on purpose: serialize() must append bytes only. If a
    str ever leaks back in, b"".join raises TypeError and this test fails
    loudly. An earlier version of the Python test encoded stray str values
    before joining, which is exactly what hid this bug for so long.
    """
    return b"".join(buf).hex(" ")


def expect(actual, wanted, label):
    if actual != wanted:
        raise Failure("%s\n  got:  %s\n  want: %s" % (label, actual, wanted))


def serialize(value):
    buf = []
    value.serialize(buf)
    return buf


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2

    gen_dir, runtime_dir = sys.argv[1], sys.argv[2]
    sys.path.insert(0, runtime_dir)
    sys.path.insert(0, gen_dir)

    from InteropFull import InteropPayload, InteropInner, InteropEmpty

    checks = 0

    # --- full payload ---
    p = InteropPayload()
    p.i8_, p.u8_, p.i16_, p.u16_ = -1, 2, -3, 4
    p.i32_, p.u32_, p.i64_, p.u64_ = -5, 6, -7, 8
    p.f32_, p.f64_, p.b_ = 1.5, 2.5, True
    p.kind_, p.text_, p.blob_ = 1, "hi", [0xAA, 0xBB]
    p.inner_ = InteropInner()
    p.inner_.x_, p.inner_.flag_ = 9, True
    p.ints_, p.words_ = [1, 2], ["a", "bc"]

    buf = serialize(p)
    data = b"".join(buf)
    expect(data.hex(" "), GOLDEN, "InteropPayload, every field set")
    checks += 1

    # Re-serializing the decoded value must reproduce the same bytes.
    back = InteropPayload()
    back.deserialize(data, 0)
    again = b"".join(serialize(back))
    expect(again.hex(" "), GOLDEN, "InteropPayload round-trip")
    checks += 1

    # --- sparse payload ---
    s = InteropPayload()
    s.i32_ = 1
    expect(b"".join(serialize(s)).hex(" "), GOLDEN_SPARSE, "InteropPayload, only i32_ set")
    checks += 1

    # --- empty struct ---
    e = InteropEmpty()
    empty_bytes = b"".join(serialize(e))
    expect(empty_bytes.hex(" "), "", "InteropEmpty must write zero bytes")
    checks += 1

    # --- UTF-8 survives the round trip (text is encoded, not byte-copied) ---
    u = InteropPayload()
    u.text_ = "中文 привет"
    uback = InteropPayload()
    uback.deserialize(b"".join(serialize(u)), 0)
    expect(uback.text_, "中文 привет", "non-ASCII text round-trip")
    checks += 1

    # --- arrays of strings and structs survive the round trip ---
    a = InteropPayload()
    a.words_ = ["", "a", "longer word"]
    a.ints_ = [-1, 0, 1]
    aback = InteropPayload()
    aback.deserialize(b"".join(serialize(a)), 0)
    expect(aback.words_, ["", "a", "longer word"], "string array round-trip")
    expect(aback.ints_, [-1, 0, 1], "int array round-trip")
    checks += 1

    print("InteropFull (Python): %d checks passed" % checks)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Failure as exc:
        print("FAIL: %s" % exc)
        sys.exit(1)
