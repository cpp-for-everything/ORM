#!/usr/bin/env python3
"""The layout check must catch what it exists to catch, and pass what is fine."""
import os
import tempfile
import unittest

import fitz

from check_layout import check


def pdf(pages):
    """A document from a list of pages, each a list of lines of text."""
    doc = fitz.open()
    for number, lines in enumerate(pages, start=1):
        page = doc.new_page()
        for row, text in enumerate(lines):
            page.insert_text((72, 72 + 14 * row), text)
        page.insert_text((300, 800), str(number))
    handle, path = tempfile.mkstemp(suffix=".pdf")
    os.close(handle)
    doc.save(path)
    return path


FULL = ["line of text"] * 50


class CheckLayout(unittest.TestCase):
    def flagged(self, pages):
        path = pdf(pages)
        try:
            return check(path, "START", "END")[2]
        finally:
            os.remove(path)

    def test_full_pages_pass(self):
        self.assertEqual(self.flagged([["START"] + FULL, FULL, ["END"]]), [])

    def test_a_stranded_line_is_flagged(self):
        found = self.flagged([["START"] + FULL, ["one line left"], ["END"]])
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0][0], 2)
        self.assertIn("near empty", found[0][1])

    def test_the_page_number_is_not_a_line_of_text(self):
        found = self.flagged([["START"] + FULL, ["a", "b", "c"], ["END"]])
        self.assertEqual(len(found), 1)

    def test_a_hole_in_the_middle_of_a_chapter_is_flagged(self):
        found = self.flagged([["START"] + FULL, ["a"] * 6, FULL, ["END"]])
        self.assertEqual(len(found), 1)
        self.assertIn("hole", found[0][1])

    def test_a_short_end_of_chapter_is_fine(self):
        found = self.flagged([["START"] + FULL, ["a"] * 6, ["2. Next chapter"] + FULL, ["END"]])
        self.assertEqual(found, [])

    def test_pages_outside_the_body_are_not_checked(self):
        found = self.flagged([["title page"], ["START"] + FULL, ["END"], ["one line"]])
        self.assertEqual(found, [])


if __name__ == "__main__":
    unittest.main()
