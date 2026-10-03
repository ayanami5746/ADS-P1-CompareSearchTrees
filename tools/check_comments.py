"""Count physical C comment lines without mistaking strings for comments."""
import argparse
import json
from pathlib import Path


def measure(path):
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    comment_lines = set()
    code_lines = set()
    state, escaped, line, i = "code", False, 0, 0
    while i < len(text):
        c = text[i]
        pair = text[i:i + 2]
        if c == "\n":
            line += 1
            if state == "line":
                state = "code"
            escaped = False
            i += 1
            continue
        if state in ("block", "line"):
            if c.strip():
                comment_lines.add(line)
            if state == "block" and pair == "*/":
                state = "code"
                i += 2
                continue
        elif state in ("string", "char"):
            code_lines.add(line)
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif (state == "string" and c == '"') or (state == "char" and c == "'"):
                state = "code"
        elif pair in ("/*", "//"):
            comment_lines.add(line)
            state = "block" if pair == "/*" else "line"
            i += 2
            continue
        else:
            if c.strip():
                code_lines.add(line)
            if c == '"':
                state = "string"
            elif c == "'":
                state = "char"
        i += 1
    # Count only standalone substantive comment lines: mixed lines get no credit.
    only = comment_lines - code_lines
    substantive = {i for i in only if lines[i].strip().strip("/* ").strip()}
    total = len(lines)
    return {"file": path.as_posix(), "total_lines": total,
            "nonblank_lines": sum(bool(s.strip()) for s in lines),
            "comment_only_lines": len(substantive),
            "ratio_all_lines": len(substantive) / total if total else 0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    files = sorted(p for folder in ("include", "src", "tests")
                   for p in Path(folder).rglob("*") if p.suffix in (".c", ".h"))
    if not files:
        raise SystemExit("No C sources found; run from the repository root.")
    rows = [measure(p) for p in files]
    total = sum(r["total_lines"] for r in rows)
    comments = sum(r["comment_only_lines"] for r in rows)
    report = {"definition": "substantive comment-only physical lines / all physical lines (including blanks)",
              "files": rows, "total_lines": total, "comment_only_lines": comments,
              "ratio": comments / total}
    for r in rows:
        print(f'{r["file"]}: {r["comment_only_lines"]}/{r["total_lines"]} = {r["ratio_all_lines"]:.2%}')
    print(f"TOTAL: {comments}/{total} = {comments / total:.2%}")
    if args.output:
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    # Enforce the threshold per source file as well as for the project overall.
    if any(r["ratio_all_lines"] < 0.35 for r in rows):
        raise SystemExit("FAIL: every C source/header must contain at least 35% comment lines")


if __name__ == "__main__":
    main()
