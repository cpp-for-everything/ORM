// Reads security/corpus/payloads.csv.
//
// RFC 4180 quoting. Control characters inside a payload are stored as the two
// character sequences \r and \n (see corpus/README.md) and are decoded here.
// No other backslash is touched, because SQL payloads use backslashes literally.
#pragma once
#include <fstream>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace orm_security
{
    struct Payload
    {
        std::string id, cls, dialect, technique, payload, expected_effect, source_id;
        // The source printed this payload with an unfilled slot such as [RANDNUM].
        // It is run exactly as published. Filling the slot would be inventing text.
        bool is_template = false;
    };

    inline std::vector<std::vector<std::string>> parse_csv(std::istream& in)
    {
        std::vector<std::vector<std::string>> rows;
        std::vector<std::string> row;
        std::string field;
        bool quoted = false;
        char c;
        while (in.get(c))
        {
            if (quoted)
            {
                if (c == '"')
                {
                    if (in.peek() == '"') { field += '"'; in.get(); }
                    else quoted = false;
                }
                else field += c;
            }
            else if (c == '"') quoted = true;
            else if (c == ',') { row.push_back(field); field.clear(); }
            else if (c == '\n')
            {
                row.push_back(field); field.clear();
                rows.push_back(row);  row.clear();
            }
            else if (c != '\r') field += c;
        }
        if (!field.empty() || !row.empty()) { row.push_back(field); rows.push_back(row); }
        return rows;
    }

    inline std::string decode_controls(const std::string& s)
    {
        std::string out;
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '\\' && i + 1 < s.size() && (s[i + 1] == 'r' || s[i + 1] == 'n'))
            {
                out += (s[i + 1] == 'r') ? '\r' : '\n';
                ++i;
            }
            else
                out += s[i];
        }
        return out;
    }

    inline std::vector<Payload> load_corpus(const std::string& path)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("cannot open corpus: " + path);
        auto rows = parse_csv(f);
        if (rows.empty()) throw std::runtime_error("empty corpus: " + path);

        std::map<std::string, std::size_t> col;
        for (std::size_t i = 0; i < rows[0].size(); ++i) col[rows[0][i]] = i;
        for (const char* k : {"id", "class", "dialect", "technique", "payload", "expected_effect", "source_id"})
            if (!col.count(k)) throw std::runtime_error(std::string("corpus is missing column: ") + k);

        const std::regex slot(R"(\[[A-Z_]+\])");
        std::vector<Payload> out;
        for (std::size_t r = 1; r < rows.size(); ++r)
        {
            const auto& v = rows[r];
            if (v.size() < rows[0].size()) continue;   // trailing blank line
            Payload p;
            p.id              = v[col["id"]];
            p.cls             = v[col["class"]];
            p.dialect         = v[col["dialect"]];
            p.technique       = v[col["technique"]];
            p.payload         = decode_controls(v[col["payload"]]);
            p.expected_effect = v[col["expected_effect"]];
            p.source_id       = v[col["source_id"]];
            p.is_template     = std::regex_search(p.payload, slot);
            out.push_back(std::move(p));
        }
        return out;
    }
} // namespace orm_security
