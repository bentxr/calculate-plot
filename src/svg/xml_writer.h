// Derived from Matplotlib 3.11.2: lib/matplotlib/backends/backend_svg.py (_XMLWriter and the
// escaping helpers), which is based on SimpleXMLWriter by Fredrik Lundh.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
//
// The SimpleXMLWriter module is
//
// Copyright (c) 2001-2004 by Fredrik Lundh
//
// By obtaining, using, and/or copying this software and/or its
// associated documentation, you agree that you have read, understood,
// and will comply with the following terms and conditions:
//
// Permission to use, copy, modify, and distribute this software and
// its associated documentation for any purpose and without fee is
// hereby granted, provided that the above copyright notice appears in
// all copies, and that both that copyright notice and this permission
// notice appear in supporting documentation, and that the name of
// Secret Labs AB or the author not be used in advertising or publicity
// pertaining to distribution of the software without specific, written
// prior permission.
//
// SECRET LABS AB AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
// TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANT-
// ABILITY AND FITNESS.  IN NO EVENT SHALL SECRET LABS AB OR THE AUTHOR
// BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
// DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
// ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
// OF THIS SOFTWARE.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::svg {

// Attributes in the order they are written; empty values are left out, as matplotlib's `if v:`.
using Attributes = std::vector<std::pair<std::string, std::string>>;

std::string escapeCdata(std::string s);
std::string escapeComment(const std::string& s);
std::string escapeAttrib(std::string s);
std::string quoteEscapeAttrib(const std::string& s);

class XmlWriter {
public:
    explicit XmlWriter(std::string& out) : out_(out) {}

    // Opens an element; returns an identifier for close().
    int start(const std::string& tag, const Attributes& attrib = {});
    void comment(const std::string& comment);
    void data(const std::string& text);
    // Closes the current element; `tag`, when given, must match it.
    void end(const std::string& tag = "", bool indent = true);
    // Closes open elements up to and including the one `id` identifies.
    void close(int id);
    // start(), data(), end(indent=false) in one.
    void element(const std::string& tag, const std::string& text = "", const Attributes& attrib = {});

private:
    void flush(bool indent = true);

    std::string& out_;
    bool open_ = false;
    std::vector<std::string> tags_;
    std::vector<std::string> data_;
    const std::string indentation_ = std::string(64, ' ');
};

}  // namespace calculate_plot::svg
