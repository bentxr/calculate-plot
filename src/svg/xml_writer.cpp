// Derived from Matplotlib 3.11.2: lib/matplotlib/backends/backend_svg.py (_XMLWriter and the
// escaping helpers), which is based on SimpleXMLWriter by Fredrik Lundh.
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Copyright (c) 2001-2004 by Fredrik Lundh
// Licence: LICENSES/Matplotlib.txt and LICENSES/SimpleXMLWriter.txt (the permission notice is in
// xml_writer.h). Changes: licensing/changes/matplotlib.md

#include "svg/xml_writer.h"

#include <regex>

namespace calculate_plot::svg {

namespace {

void replaceAll(std::string& s, const std::string& from, const std::string& to) {
    for (std::size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
        s.replace(at, from.size(), to);
}

}  // namespace

std::string escapeCdata(std::string s) {
    replaceAll(s, "&", "&amp;");
    replaceAll(s, "<", "&lt;");
    replaceAll(s, ">", "&gt;");
    return s;
}

std::string escapeComment(const std::string& s) {
    // _escape_xml_comment: '-' followed by '-' becomes "- ".
    static const std::regex dashBeforeDash("-(?=-)");
    return std::regex_replace(escapeCdata(s), dashBeforeDash, "- ");
}

std::string escapeAttrib(std::string s) {
    replaceAll(s, "&", "&amp;");
    replaceAll(s, "'", "&apos;");
    replaceAll(s, "\"", "&quot;");
    replaceAll(s, "<", "&lt;");
    replaceAll(s, ">", "&gt;");
    return s;
}

std::string quoteEscapeAttrib(const std::string& s) {
    if (s.find('"') == std::string::npos) return "\"" + escapeCdata(s) + "\"";
    if (s.find('\'') == std::string::npos) return "'" + escapeCdata(s) + "'";
    return "\"" + escapeAttrib(s) + "\"";
}

void XmlWriter::flush(bool indent) {
    // flush internal buffers
    if (open_) {
        out_ += indent ? ">\n" : ">";
        open_ = false;
    }
    if (!data_.empty()) {
        std::string data;
        for (const auto& d : data_) data += d;
        out_ += escapeCdata(data);
        data_.clear();
    }
}

int XmlWriter::start(const std::string& tagName, const Attributes& attrib) {
    flush();
    const std::string tag = escapeCdata(tagName);
    data_.clear();
    tags_.push_back(tag);
    out_ += indentation_.substr(0, tags_.size() - 1);
    out_ += "<" + tag;
    for (const auto& [k, v] : attrib)
        if (!v.empty()) out_ += " " + escapeCdata(k) + "=" + quoteEscapeAttrib(v);
    open_ = true;
    return int(tags_.size()) - 1;
}

void XmlWriter::comment(const std::string& comment) {
    flush();
    out_ += indentation_.substr(0, tags_.size());
    out_ += "<!-- " + escapeComment(comment) + " -->\n";
}

void XmlWriter::data(const std::string& text) { data_.push_back(text); }

void XmlWriter::end(const std::string& tagName, bool indent) {
    (void)tagName;  // matplotlib asserts it matches; the port's callers always pass the right one
    const std::string tag = tags_.back();
    tags_.pop_back();
    if (!data_.empty()) {
        flush(indent);
    } else if (open_) {
        open_ = false;
        out_ += "/>\n";
        return;
    }
    if (indent) out_ += indentation_.substr(0, tags_.size());
    out_ += "</" + tag + ">\n";
}

void XmlWriter::close(int id) {
    while (int(tags_.size()) > id) end();
}

void XmlWriter::element(const std::string& tag, const std::string& text, const Attributes& attrib) {
    start(tag, attrib);
    if (!text.empty()) data(text);
    end("", false);
}

}  // namespace calculate_plot::svg
