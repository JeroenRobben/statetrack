#!/usr/bin/env python3
"""
extract_value_names.py - static enum/macro value->name maps for statetrack state vars.

Usage:
  extract_value_names.py --compile-db compile_commands.json [-o out.json]
"""

import argparse
import glob
import json
import os
import sys

import clang.cindex as ci
from ctypes import c_int, c_longlong, c_char_p, c_void_p

CK = ci.CursorKind


# --- libclang wiring -------------------------------------------------------


def load_libclang():
    if ci.Config.loaded:
        return
    for cand in ["/usr/lib/libclang.so"] + sorted(glob.glob("/usr/lib/libclang.so.*")):
        if os.path.exists(cand):
            try:
                ci.Config.set_library_file(cand)
                return
            except Exception:
                pass
    # else fall through and let cindex autoload the default name


# --- ctypes bridge to the unwrapped clang_Cursor_Evaluate family -----------

_bridges_ready = False
_HAS_BINOP_KIND = False

# CXBinaryOperatorKind values we harvest a constant from: ==(15), !=(16), =(22).
# Bitwise/compound ops (& | |= ...) are excluded so flag fields stay numeric.
_HARVEST_OPS = {15, 16, 22}


def _register_bridges():
    """Wire the unwrapped C APIs the bindings omit: clang_Cursor_Evaluate (const
    folding) and clang_getCursorBinaryOperatorKind (operator discrimination)."""
    global _bridges_ready, _HAS_BINOP_KIND
    if _bridges_ready:
        return
    lib = ci.conf.lib
    lib.clang_Cursor_Evaluate.argtypes = [ci.Cursor]
    lib.clang_Cursor_Evaluate.restype = c_void_p
    lib.clang_EvalResult_getKind.argtypes = [c_void_p]
    lib.clang_EvalResult_getKind.restype = c_int
    lib.clang_EvalResult_getAsLongLong.argtypes = [c_void_p]
    lib.clang_EvalResult_getAsLongLong.restype = c_longlong
    lib.clang_EvalResult_getAsStr.argtypes = [c_void_p]
    lib.clang_EvalResult_getAsStr.restype = c_char_p
    lib.clang_EvalResult_dispose.argtypes = [c_void_p]
    try:
        lib.clang_getCursorBinaryOperatorKind.argtypes = [ci.Cursor]
        lib.clang_getCursorBinaryOperatorKind.restype = c_int
        _HAS_BINOP_KIND = True
    except AttributeError:
        _HAS_BINOP_KIND = False  # old libclang: fall back to harvest-all
    _bridges_ready = True


def evaluate(cursor):
    """Fold a constant-expression cursor -> int | str | None."""
    lib = ci.conf.lib
    res = lib.clang_Cursor_Evaluate(cursor)
    if not res:
        return None
    try:
        kind = lib.clang_EvalResult_getKind(res)
        if kind == 1:  # CXEval_Int
            return int(lib.clang_EvalResult_getAsLongLong(res))
        if kind in (3, 4, 5):  # string-literal kinds
            s = lib.clang_EvalResult_getAsStr(res)
            return s.decode() if s else None
        return None  # Float/Other/UnExposed
    finally:
        lib.clang_EvalResult_dispose(res)


# --- parsing a compile database --------------------------------------------


def _clang_args(argv, path):
    """Keep the flags cindex needs; drop the compiler, -c, -o OUT and the source."""
    out, skip = [], False
    base = os.path.basename(path)
    for i, a in enumerate(argv):
        if i == 0 or skip:  # argv[0]=compiler, or -o operand
            skip = False
            continue
        if a == "-c":
            continue
        if a == "-o":
            skip = True
            continue
        if a == path or os.path.basename(a) == base:
            continue
        out.append(a)
    return out


def _resource_dir():
    """Clang's builtin-header dir (holds stddef.h etc.). Without it on the parse args,
    libclang fatally fails on <stddef.h>, the TU never builds, and we silently find zero
    registrations. Resolved against the SYSTEM clang -- it must match libclang, not the
    target's build compiler (we only parse, never compile)."""
    try:
        import subprocess

        rd = subprocess.run(
            ["clang", "-print-resource-dir"], capture_output=True, text=True
        ).stdout.strip()
        if rd and os.path.isdir(rd):
            return rd
    except (OSError, ValueError):
        pass
    cands = sorted(glob.glob("/usr/lib/clang/*"))  # fall back to a versioned dir
    return cands[-1] if cands else None


def parse_db(db_path):
    db = json.load(open(db_path))
    idx = ci.Index.create()
    opts = ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
    rd = _resource_dir()
    tus = []
    for e in db:
        directory = e.get("directory", ".")
        f = e["file"]
        path = f if os.path.isabs(f) else os.path.normpath(os.path.join(directory, f))
        argv = e["arguments"] if "arguments" in e else e["command"].split()
        args = _clang_args(argv, path)
        if rd and "-resource-dir" not in args:  # don't override an explicit one
            args += ["-resource-dir", rd]
        tus.append(idx.parse(path, args=args, options=opts))
    return tus


# --- seed collection (STATETRACK_REGISTER call sites) ----------------------


def _registered_decl(arg):
    """1st arg of statetrack_register is &(expr); return the referenced Field/Var decl."""
    for c in arg.walk_preorder():
        r = c.referenced
        if r is not None and r.kind in (CK.FIELD_DECL, CK.VAR_DECL):
            return r
    return None


def _label_of(arg):
    """3rd arg is the runtime label string literal; recover its value robustly."""
    v = evaluate(arg)
    if isinstance(v, str):
        return v
    for c in arg.walk_preorder():
        if c.kind == CK.STRING_LITERAL:
            v = evaluate(c)
            if isinstance(v, str):
                return v
            sp = c.spelling
            if sp.startswith('"') and sp.endswith('"'):
                return sp[1:-1]
    return None


def _enum_family(decl_type):
    """For an enum-typed decl, the family is the enum's constants (name<->value)."""
    fam = {}
    ed = decl_type.get_declaration()
    for c in ed.get_children():
        if c.kind == CK.ENUM_CONSTANT_DECL:
            fam[c.enum_value] = c.spelling
    return fam


def collect_seeds(tu, seeds):
    """Find statetrack_register calls; record each variable keyed by canonical USR."""
    for c in tu.cursor.walk_preorder():
        if c.kind != CK.CALL_EXPR:
            continue
        name = c.spelling or (c.referenced.spelling if c.referenced else "")
        if name != "statetrack_register":
            continue
        args = list(c.get_arguments())
        if len(args) < 3:
            continue
        decl = _registered_decl(args[0])
        if decl is None:
            continue
        usr = decl.get_usr()
        t = decl.type.get_canonical()
        s = seeds.setdefault(
            usr, {"label": None, "is_enum": t.kind == ci.TypeKind.ENUM, "family": {}}
        )
        if s["label"] is None:
            s["label"] = _label_of(args[2]) or decl.spelling
        if s["is_enum"] and not s["family"]:
            s["family"] = _enum_family(t)


# --- use-site analysis (int / macro-coded fields) --------------------------


def _loc_key(loc):
    return (loc.file.name if loc.file else None, loc.offset)


def build_macro_map(tu):
    """Map each macro-expansion site (file, offset) -> macro name. Needed because
    get_tokens() is unusable on macro-expanded extents (returns whole-file garbage)."""
    m = {}
    for c in tu.cursor.walk_preorder():
        if c.kind == CK.MACRO_INSTANTIATION:
            m.setdefault(_loc_key(c.extent.start), c.spelling)
    return m


def _ref_usr(cursor, seed_usrs):
    """Return a seed USR referenced anywhere under `cursor`, else None."""
    for c in cursor.walk_preorder():
        r = c.referenced
        if r is not None and r.get_usr() in seed_usrs:
            return r.get_usr()
    return None


def _const_name_value(expr, macro_at):
    """A constant operand -> (name, int) if it carries a symbolic name, else (None, None).
    Bare literals have no name and are intentionally dropped (stay numeric)."""
    r = expr.referenced
    if r is not None and r.kind == CK.ENUM_CONSTANT_DECL:
        return r.spelling, r.enum_value
    name = macro_at.get(_loc_key(expr.extent.start))
    val = evaluate(expr)
    if name is not None and isinstance(val, int):
        return name, val
    return None, None


def _harvest_switch(sw, seed_usrs, macro_at, seeds):
    kids = list(sw.get_children())
    if not kids:
        return
    usr = _ref_usr(kids[0], seed_usrs)  # the scrutinee variable
    if usr is None:
        return
    for c in sw.walk_preorder():
        if c.kind == CK.CASE_STMT:
            ck = list(c.get_children())
            if ck:
                name, val = _const_name_value(ck[0], macro_at)
                if name is not None:
                    seeds[usr]["family"][val] = name


def _harvest_binop(op, seed_usrs, macro_at, seeds):
    """A binop with the seed on one side and a named constant on the other
    (x == K, x = K) contributes K to the seed's family. Bitwise/compound ops
    (e.g. a flag test `flags & F`) are excluded so flag fields stay numeric."""
    if (
        _HAS_BINOP_KIND
        and ci.conf.lib.clang_getCursorBinaryOperatorKind(op) not in _HARVEST_OPS
    ):
        return
    kids = list(op.get_children())
    if len(kids) != 2:
        return
    a, b = _ref_usr(kids[0], seed_usrs), _ref_usr(kids[1], seed_usrs)
    if a and not b:
        usr, other = a, kids[1]
    elif b and not a:
        usr, other = b, kids[0]
    else:
        return
    name, val = _const_name_value(other, macro_at)
    if name is not None:
        seeds[usr]["family"][val] = name


def sweep_usesites(tu, seeds, macro_at):
    seed_usrs = set(seeds)
    for c in tu.cursor.walk_preorder():
        if c.kind == CK.SWITCH_STMT:
            _harvest_switch(c, seed_usrs, macro_at, seeds)
        elif c.kind == CK.BINARY_OPERATOR:
            _harvest_binop(c, seed_usrs, macro_at, seeds)


# --- emit ------------------------------------------------------------------


def build_output(seeds):
    decode, numeric = {}, []
    for s in seeds.values():
        label = s["label"]
        if s["family"]:
            decode[label] = {str(v): n for v, n in s["family"].items()}
        else:
            numeric.append(label)
    return {"decode": decode, "numeric": sorted(numeric)}


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--compile-db", required=True, help="path to compile_commands.json")
    ap.add_argument("-o", "--output", help="write JSON here (default: stdout)")
    args = ap.parse_args()

    load_libclang()
    tus = parse_db(args.compile_db)
    _register_bridges()
    seeds = {}
    for tu in tus:
        collect_seeds(tu, seeds)
    for tu in tus:  # all TUs, merged by USR
        sweep_usesites(tu, seeds, build_macro_map(tu))

    text = json.dumps(build_output(seeds), indent=2, sort_keys=True)
    if args.output:
        with open(args.output, "w") as f:
            f.write(text + "\n")
    else:
        print(text)


if __name__ == "__main__":
    main()
