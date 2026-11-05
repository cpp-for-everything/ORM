#!/usr/bin/env python3
"""Find pages of the thesis that a reader would call a layout flaw.

A clean LaTeX build says nothing about this: the page that held one line of the
conclusion built without a warning. So this reads the PDF, the thing that is printed.

Two rules, applied from the first body page to the end of the document:

  near empty   at most MIN_LINES lines of body text
  hole         the page is less than MIN_FILL full, and the next page does not start
               a new chapter, so the gap is not the natural end of one

The running page number is not body text. Chapters may end anywhere, so a short last
page of a chapter passes the second rule, but not the first: one stranded line is a
flaw wherever it falls.

Usage: check_layout.py <file.pdf> [--first-body-text TEXT] [--end-text TEXT]
Exit status 0 when no page is flagged, 1 otherwise.
"""
import argparse
import re
import sys

import fitz  # PyMuPDF

MIN_LINES = 3
MIN_FILL = 0.25
PAGE_NUMBER = re.compile(r"^\s*\d+\s*$")
# "1. Увод", "3. Съществуващи решения" and the unnumbered chapters in capitals.
CHAPTER_START = re.compile(r"^\s*(\d+\.\s+\S|[А-Я]{5,}\s*$)")


def body_lines(page):
    """Lines of text on a page as (top, bottom, text), without the page number."""
    lines = []
    for block in page.get_text("dict")["blocks"]:
        for line in block.get("lines", []):
            text = "".join(span["text"] for span in line["spans"]).strip()
            if text:
                lines.append((line["bbox"][1], line["bbox"][3], text))
    lines.sort()
    if lines and PAGE_NUMBER.match(lines[-1][2]):
        lines.pop()
    return lines


def starts_chapter(page):
    lines = body_lines(page)
    return bool(lines) and bool(CHAPTER_START.match(lines[0][2]))


def text_area(doc, first, last):
    """Top and bottom of the text block, as the extremes over the body pages."""
    tops, bottoms = [], []
    for number in range(first, last + 1):
        lines = body_lines(doc[number])
        if lines:
            tops.append(lines[0][0])
            bottoms.append(lines[-1][1])
    return min(tops), max(bottoms)


def first_body_page(doc, marker):
    for number in range(len(doc)):
        lines = body_lines(doc[number])
        if lines and lines[0][2].upper().startswith(marker.upper()):
            return number
    return 0


def last_body_page(doc, marker):
    """The page before the first page that starts with the marker, if it exists."""
    for number in range(len(doc)):
        lines = body_lines(doc[number])
        if lines and lines[0][2].upper().startswith(marker.upper()):
            return number - 1
    return len(doc) - 1


def check(path, first_marker="УВОД", end_marker=None):
    doc = fitz.open(path)
    first = first_body_page(doc, first_marker)
    last = last_body_page(doc, end_marker) if end_marker else len(doc) - 1
    top, bottom = text_area(doc, first, last)
    flagged = []
    for number in range(first, last + 1):
        lines = body_lines(doc[number])
        fill = (lines[-1][1] - top) / (bottom - top) if lines else 0.0
        label = number + 1
        if len(lines) <= MIN_LINES:
            flagged.append((label, f"near empty: {len(lines)} lines of body text"))
        elif fill < MIN_FILL and number < last and not starts_chapter(doc[number + 1]):
            flagged.append((label, f"hole: {fill:.0%} full, and no chapter starts next"))
    return first + 1, last + 1, flagged


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("pdf")
    parser.add_argument("--first-body-text", default="УВОД")
    parser.add_argument("--end-text", default=None)
    args = parser.parse_args()
    first, last, flagged = check(args.pdf, args.first_body_text, args.end_text)
    print(f"checked PDF pages {first} to {last}")
    for page, why in flagged:
        print(f"PDF page {page}: {why}")
    print("layout: " + ("FAILED" if flagged else "ok"))
    return 1 if flagged else 0


if __name__ == "__main__":
    sys.exit(main())
