from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


screen = read("apps/crazypod/ui/features/books/crazypod_books_screen.c")
header = read("apps/crazypod/ui/features/books/crazypod_books_screen.h")
workflow = read("apps/crazypod/ui/features/books/crazypod_books_workflow.c")
books = read("apps/crazypod/crazypod_books.c")
cache = read("apps/crazypod/epub/cache/crazypod_epub_cache.c")
epub = read("apps/crazypod/crazypod_epub.c")

assert "#define CRAZYPOD_BOOKS_READER_MARGIN 10" in header
assert "#define CRAZYPOD_BOOKS_READER_LINE_SPACE 2" in header
assert "reader_font_12.fallback = &reader_font_14" in screen
assert "reader_font_14.fallback = &lv_font_unscii_16" in screen
assert "reader_font_16.fallback = &reader_font_14" in screen
assert "lv_font_get_glyph_dsc" in screen
assert "reader_sanitize_line(page_text" in screen
assert "Reflowing Text" in workflow
assert "crazypod_book_session_begin(index)" in workflow
assert "BOOK_TEXT_ENCODING_UTF16LE" in books
assert "BOOK_TEXT_ENCODING_UTF16BE" in books
assert "Unknown text encoding." in books
assert "EPUB_CACHE_VERSION_LEGACY 6u" in cache
assert "Cache invalidation must not erase" in books
assert "progress = 0" not in workflow
for extension in (".png", ".gif", ".webp"):
    assert f'"{extension}"' in epub
assert "Image unavailable" in screen

print("CrazyPod reader font, encoding, cache, and placeholder contracts pass")
