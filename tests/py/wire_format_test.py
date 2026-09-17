"""Byte-level conformance test for the generated Python encoder.

The C++ golden vectors in tests/runtime/wire_format_golden_test.cpp pin the
wire format itself, but they cannot catch a Python-only regression: the
nested-struct mask bug lived entirely in PYGenerator's output and every C++
test stayed green through it. This runs the *generated Python* against the
same byte sequences.

Usage:
    wire_format_test.py <generated_py_dir> <runtime_py_dir>

Exits non-zero with a diff on the first mismatch. No test framework needed --
plain asserts, so it runs anywhere Python 3 does.
"""

import sys
import os


def load(gen_dir, runtime_dir):
    # The generated code does `from rpc.writer import *`, and the runtime ships
    # as the `rpc` package. Put the runtime's parent on the path so that import
    # resolves to the source tree rather than to an installed copy.
    sys.path.insert(0, runtime_dir)
    sys.path.insert(0, gen_dir)


def to_hex(buf):
    """serialize() appends a mix of bytes and str (the runtime is Py2-flavoured),
    so normalise before joining -- the same dance a caller has to do."""
    return b"".join(
        x if isinstance(x, bytes) else x.encode("latin-1") for x in buf
    ).hex(" ")


def serialize(value):
    buf = []
    value.serialize(buf)
    return to_hex(buf)


class Failure(Exception):
    pass


def expect(actual, expected, what):
    if actual != expected:
        raise Failure(
            "%s\n     expected: %s\n     actual:   %s" % (what, expected, actual)
        )


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2

    gen_dir, runtime_dir = sys.argv[1], sys.argv[2]
    load(gen_dir, runtime_dir)

    from Nested import Inner, Outer, Holder

    checks = 0

    # --- nested struct takes exactly one mask bit of its parent ---------------
    v = Outer()
    v.in_.x_ = 7
    v.in_.b_ = True
    v.y_ = 3
    expect(serialize(v), "01 c0 01 c0 07 00 00 00 03 00 00 00",
           "nested struct with a non-default sibling")
    checks += 1

    # The sub-struct's bit stays set even when the following field is default.
    # Emitting nothing for it (the old behaviour) shifted y_'s bit down to
    # position 0 and produced 01 00 here.
    v = Outer()
    v.in_.x_ = 7
    v.y_ = 0
    expect(serialize(v), "01 80 01 80 07 00 00 00",
           "nested struct with a defaulted sibling")
    checks += 1

    # A nested struct is always written, so the parent mask keeps bit0 even
    # when everything is at its default.
    expect(serialize(Outer()), "01 80 01 00", "nested struct, all defaults")
    checks += 1

    # --- array elements carry no mask ----------------------------------------
    h = Holder()
    a = Inner()
    a.x_ = 1
    c = Inner()
    c.x_ = 2
    h.items_ = [a, c]
    h.z_ = 5
    expect(serialize(h),
           "01 c0 02 01 80 01 00 00 00 01 80 02 00 00 00 05 00 00 00",
           "array of nested structs")
    checks += 1

    expect(serialize(Holder()), "01 00", "holder, all defaults")
    checks += 1

    # --- round trip ----------------------------------------------------------
    for tag, value, hexed in [
        ("Outer", v, "01 80 01 80 07 00 00 00"),
        ("Holder", h, "01 c0 02 01 80 01 00 00 00 01 80 02 00 00 00 05 00 00 00"),
    ]:
        data = bytes.fromhex(hexed.replace(" ", ""))
        back = Outer() if tag == "Outer" else Holder()
        back.deserialize(data, 0)
        expect(serialize(back), hexed, "%s round trip changed the bytes" % tag)
        checks += 1

    print("Python wire format: %d checks passed" % checks)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Failure as e:
        print("FAIL: %s" % e)
        sys.exit(1)
