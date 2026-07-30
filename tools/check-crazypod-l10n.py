#!/usr/bin/env python3
"""Audit CrazyPod localization resources and C call sites.

The parser below is deliberately a small C lexer, not a regular expression over
source text.  It understands comments, character literals, escaped strings,
balanced macro arguments and adjacent C string literal concatenation.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

LOCALES = ("zh-Hans", "zh-Hant", "ja", "ko", "de", "fr", "es", "pt-BR")
PRINTF = re.compile(
    r"%(?:\d+\$)?[-+ #0']*(?:\d+|\*)?(?:\.(?:\d+|\*))?"
    r"(?:hh|h|ll|l|j|z|t|L)?[diuoxXfFeEgGaAcspn%]"
)
ENGLISH = re.compile(r"[A-Za-z]{3,}")
SINK_PARTS = (
    "draw_text", "draw_label", "draw_title", "draw_subtitle", "draw_message",
    "lcd_puts", "lcd_putsf", "splash", "set_message", "show_message",
    "choice_overlay", "confirm_overlay", "snprintf",
)
MINIAPP_SOURCES = {
    "Error", "Cannot divide by zero", "Result out of range",
    " OF ", " MIN", " ROUNDS", "ROUNDS", "SHORT BREAK", "LONG BREAK",
    "FOCUS", "SHORT", "LONG", "RUNNING", "PAUSED", "COMPLETE", "READY",
    "START", "SETUP", "PAUSE", "SKIP", "RESET", "RESUME", "NEXT",
    "TIMER NOT STARTED", "CHANGES NOT SAVED", "DONE", "Settings saved",
}


@dataclass(frozen=True)
class Token:
    kind: str
    text: str
    line: int
    start: int
    end: int


def lex_c(text: str) -> list[Token]:
    out: list[Token] = []
    i, line, n = 0, 1, len(text)
    while i < n:
        ch = text[i]
        if ch.isspace():
            line += ch == "\n"
            i += 1
            continue
        if text.startswith("//", i):
            end = text.find("\n", i + 2)
            i = n if end < 0 else end
            continue
        if text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n - 2 if end < 0 else end
            line += text.count("\n", i, end + 2)
            i = end + 2
            continue
        start, start_line = i, line
        prefix = ""
        if text.startswith("u8\"", i):
            prefix, i = "u8", i + 2
        elif ch in "uUL" and i + 1 < n and text[i + 1] == '"':
            prefix, i = ch, i + 1
        if text[i] == '"':
            i += 1
            while i < n:
                if text[i] == "\\":
                    if i + 1 < n and text[i + 1] == "\n":
                        line += 1
                    i += 2
                elif text[i] == '"':
                    i += 1
                    break
                else:
                    line += text[i] == "\n"
                    i += 1
            out.append(Token("string", text[start:i], start_line, start, i))
            continue
        if ch == "'":
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                elif text[i] == "'":
                    i += 1
                    break
                else:
                    line += text[i] == "\n"
                    i += 1
            out.append(Token("char", text[start:i], start_line, start, i))
            continue
        if ch.isalpha() or ch == "_":
            i += 1
            while i < n and (text[i].isalnum() or text[i] == "_"):
                i += 1
            out.append(Token("id", text[start:i], start_line, start, i))
            continue
        out.append(Token("punct", ch, start_line, start, start + 1))
        i += 1
    return out


def decode_c_string(token: str) -> str:
    first = token.find('"')
    body = token[first + 1:-1]
    out, i = bytearray(), 0
    simple = {
        "a": "\a", "b": "\b", "f": "\f", "n": "\n", "r": "\r",
        "t": "\t", "v": "\v", "\\": "\\", "'": "'", '"': '"', "?": "?",
    }
    while i < len(body):
        if body[i] != "\\":
            out.extend(body[i].encode("utf-8"))
            i += 1
            continue
        i += 1
        if i >= len(body):
            raise ValueError("unterminated escape")
        ch = body[i]
        if ch == "\n":
            i += 1
        elif ch in simple:
            out.extend(simple[ch].encode("utf-8"))
            i += 1
        elif ch in "01234567":
            j = i + 1
            while j < min(i + 3, len(body)) and body[j] in "01234567":
                j += 1
            out.append(int(body[i:j], 8) & 0xff)
            i = j
        elif ch == "x":
            j = i + 1
            while j < len(body) and body[j] in "0123456789abcdefABCDEF":
                j += 1
            if j == i + 1:
                raise ValueError(r"\x without hexadecimal digits")
            value = int(body[i + 1:j], 16)
            if value > 0xff:
                raise ValueError(r"\x escape exceeds one byte")
            out.append(value)
            i = j
        elif ch in "uU":
            count = 4 if ch == "u" else 8
            digits = body[i + 1:i + 1 + count]
            if len(digits) != count or not all(c in "0123456789abcdefABCDEF" for c in digits):
                raise ValueError(f"invalid \\{ch} escape")
            out.extend(chr(int(digits, 16)).encode("utf-8"))
            i += count + 1
        else:
            raise ValueError(f"unsupported C escape \\{ch}")
    try:
        return out.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ValueError(f"string is not valid UTF-8 after C escape decoding: {exc}") from exc


def matching_paren(tokens: list[Token], opening: int) -> int | None:
    depth = 0
    for i in range(opening, len(tokens)):
        if tokens[i].text == "(":
            depth += 1
        elif tokens[i].text == ")":
            depth -= 1
            if depth == 0:
                return i
    return None


def macro_keys(tokens: list[Token], path: Path) -> tuple[dict[str, list[str]], list[str], set[int]]:
    found: dict[str, list[str]] = {}
    errors: list[str] = []
    covered: set[int] = set()
    for i, tok in enumerate(tokens[:-1]):
        if tok.kind != "id" or tok.text not in ("CP_TR", "CP_FMT") or tokens[i + 1].text != "(":
            continue
        same_line = [t for t in tokens if t.line == tok.line]
        for j in range(len(same_line) - 2):
            if (
                same_line[j].text == "#"
                and same_line[j + 1].text == "define"
                and same_line[j + 2].text in ("CP_TR", "CP_FMT")
            ):
                break
        else:
            j = -1
        if j >= 0:
            continue  # inside the CP_TR/CP_FMT definitions themselves
        end = matching_paren(tokens, i + 1)
        if end is None:
            errors.append(f"{path}:{tok.line}: unterminated {tok.text} call")
            continue
        args = tokens[i + 2:end]
        strings = [(j, t) for j, t in enumerate(args) if t.kind == "string"]
        if not strings or any(t.kind != "string" for t in args):
            errors.append(
                f"{path}:{tok.line}: {tok.text} argument must consist only of "
                "adjacent string literals"
            )
            continue
        try:
            key = "".join(decode_c_string(t.text) for _, t in strings)
        except ValueError as exc:
            errors.append(f"{path}:{tok.line}: {exc}")
            continue
        found.setdefault(key, []).append(f"{path.as_posix()}:{tok.line}")
        covered.update(range(i, end + 1))
    return found, errors, covered


def function_name(tokens: list[Token], opening: int) -> str:
    return tokens[opening - 1].text if opening and tokens[opening - 1].kind == "id" else ""


def looks_user_facing(value: str) -> bool:
    if not ENGLISH.search(value) or len(value.strip()) < 2:
        return False
    if value.startswith(("/", ".", "%", "<")):
        return False
    if re.fullmatch(r"[A-Za-z0-9_./:+%*\-]+", value) and (
        "/" in value or "." in value or "_" in value
    ):
        return False
    return True


def bare_sink_strings(tokens: list[Token], path: Path, covered: set[int]) -> list[str]:
    warnings: list[str] = []
    for opening, token in enumerate(tokens):
        if token.text != "(":
            continue
        name = function_name(tokens, opening)
        if not name or not any(part in name.lower() for part in SINK_PARTS):
            continue
        if name == "snprintf" and "/ui/" not in f"/{path.as_posix()}":
            continue
        end = matching_paren(tokens, opening)
        if end is None:
            continue
        for i in range(opening + 1, end):
            tok = tokens[i]
            if i in covered or tok.kind != "string":
                continue
            try:
                value = decode_c_string(tok.text)
            except ValueError:
                continue
            if looks_user_facing(value):
                warnings.append(
                    f"{path}:{tok.line}: unmarked UI string in {name}(): {value!r}"
                )
    return warnings


def placeholders(text: str) -> list[str]:
    return [x for x in PRINTF.findall(text) if x != "%%"]


def load_object(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected a JSON object")
    return value


def audit(repo: Path, strict_bare: bool) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []
    locale_root = repo / "localization/crazypod"
    catalog_doc = load_object(locale_root / "catalog.json")
    catalog = catalog_doc.get("strings")
    if not isinstance(catalog, dict):
        return [f"{locale_root / 'catalog.json'}: missing object field 'strings'"], []
    for key, metadata in catalog.items():
        if not isinstance(key, str) or not key:
            errors.append("localization/crazypod/catalog.json: empty/non-string key")
            continue
        if not isinstance(metadata, dict):
            errors.append(
                f"localization/crazypod/catalog.json: metadata is not an object: {key!r}"
            )
            continue
        if metadata.get("keepEnglish") not in (None, True, False):
            errors.append(
                f"localization/crazypod/catalog.json: invalid keepEnglish: {key!r}"
            )
        locations = metadata.get("locations")
        if not isinstance(locations, list) or not all(
            isinstance(item, str) and item for item in locations
        ):
            errors.append(
                f"localization/crazypod/catalog.json: invalid locations: {key!r}"
            )

    calls: dict[str, list[str]] = {}
    roots = (repo / "apps/crazypod", repo / "miniapps")
    for root in roots:
        if not root.exists():
            continue
        for path in sorted(root.rglob("*.[ch]")):
            tokens = lex_c(path.read_text(encoding="utf-8", errors="replace"))
            keys, parse_errors, covered = macro_keys(tokens, path.relative_to(repo))
            errors.extend(parse_errors)
            for key, locations in keys.items():
                calls.setdefault(key, []).extend(locations)
            warnings.extend(bare_sink_strings(tokens, path.relative_to(repo), covered))

    for key, locations in sorted(calls.items()):
        if key not in catalog:
            errors.append(f"{locations[0]}: localization key absent from catalog: {key!r}")
    for key in sorted(set(catalog) - set(calls) - MINIAPP_SOURCES):
        errors.append(f"localization/crazypod/catalog.json: unused key: {key!r}")

    expected = set(catalog)
    for locale in LOCALES:
        path = locale_root / f"{locale}.json"
        values = load_object(path)
        for key in sorted(set(values) - expected):
            errors.append(f"{path.relative_to(repo)}: unknown key: {key!r}")
        for key in sorted(expected - set(values)):
            if not catalog[key].get("keepEnglish"):
                errors.append(f"{path.relative_to(repo)}: missing key: {key!r}")
        for key in sorted(expected & set(values)):
            value = values[key]
            keep = bool(catalog[key].get("keepEnglish"))
            if value == "__TODO__":
                errors.append(f"{path.relative_to(repo)}: TODO: {key!r}")
            elif not isinstance(value, str) or not value:
                errors.append(f"{path.relative_to(repo)}: empty/non-string value: {key!r}")
            elif keep and value != key:
                errors.append(f"{path.relative_to(repo)}: keepEnglish value changed: {key!r}")
            elif not keep and placeholders(value) != placeholders(key):
                errors.append(
                    f"{path.relative_to(repo)}: placeholders differ for {key!r}: "
                    f"{placeholders(key)!r} != {placeholders(value)!r}"
                )

    # De-duplicate warnings produced by nested sink calls.
    warnings = list(dict.fromkeys(warnings))
    if strict_bare:
        errors.extend(warnings)
        warnings = []
    return errors, warnings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path("."))
    parser.add_argument(
        "--strict-bare", action="store_true",
        help="treat unmarked strings in common UI sinks as errors",
    )
    args = parser.parse_args()
    try:
        errors, warnings = audit(args.repo.resolve(), args.strict_bare)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    for item in warnings:
        print(f"warning: {item}")
    for item in errors:
        print(f"error: {item}", file=sys.stderr)
    print(f"CrazyPod l10n audit: {len(errors)} error(s), {len(warnings)} warning(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
