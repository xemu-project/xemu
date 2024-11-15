// Copyright (c) 2022 Matt Borgerson
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdio.h>
#include <limits.h>
#include <float.h>
#include <string.h>
#include <assert.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

// 3rdparty
#include <toml++/toml.h>

#ifndef DEBUG
#define DEBUG 0
#endif

template< class T >
std::unique_ptr<T> copy_unique(const std::unique_ptr<T>& source)
{
    return source ? std::make_unique<T>(*source) : nullptr;
}

template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

typedef enum CNodeType {
    Array, Boolean, Enum, Integer, Number, String, Table,
} CNodeType;

char const * const type_names[] = {
    "Array", "Boolean", "Enum", "Integer", "Number", "String", "Table",
};

class CNode {
public:
    CNodeType type;
    std::string name;
    std::vector<CNode> children;
    union {
        struct { bool val, default_val; } boolean;
        struct { float min, max, val, default_val; } number;
        struct { int min, max, val, default_val; } integer;
    } data;
    struct { std::string val, default_val; } string;
    std::unique_ptr<CNode> array_item_type;
    struct { std::vector<std::string> values; int val, default_val; } data_enum;

    struct {
        size_t offset, count_offset, size;
    } serialized;

    CNode(const CNode& other)
    {
        type = other.type;
        name = std::string(other.name);
        children = std::vector<CNode>(other.children);
        array_item_type = copy_unique(other.array_item_type);
        data = other.data;
        data_enum = other.data_enum;
        string = other.string;
        serialized = other.serialized;
    }

    CNode& operator=(const CNode& other)
    {
        type = other.type;
        name = std::string(other.name);
        children = std::vector<CNode>(other.children);
        array_item_type = copy_unique(other.array_item_type);
        data = other.data;
        data_enum = other.data_enum;
        string = other.string;
        serialized = other.serialized;

        return *this;
    }

    CNode(std::string name, std::vector<CNode> children = {})
    : type(CNodeType::Table), name(name), children(children) {}

    CNode(std::string name, CNodeType type)
    : type(type), name(name) {}

    //
    // Get child node by name
    //
    CNode *child(std::string_view needle) {
        for (auto &c : children) {
            if (!needle.compare(c.name)) {
                return &c;
            }
        }
        return NULL;
    }

    //
    // Map the enumerated type `value` to the corresponding integer
    //
    int enum_str_to_int(std::string value)
    {
        auto &v = data_enum.values;
        auto it = std::find(v.begin(), v.end(), value);
        return (it != v.end()) ? it - v.begin() : -1;
    }

    //
    // Print indentation to stdout
    //
    void indent(int c) {
        while (c--) printf("  ");
    }

    //
    // Print a representation of the tree to stdout
    //
    void repr(int depth = 0) {
        indent(depth); printf("%s<%s> ", name.c_str(), type_names[type]);

        if (type == Table) {
            printf("\n");
            for (auto &c : children) {
                c.repr(depth + 1);
            }
            return;
        }

        printf("@%zd ", serialized.offset);

        const char *tf_str[2] = { "false", "true" };
        switch (type) {
            case Array:
                printf("%zdB {\n", serialized.size);
                array_item_type->repr(depth+1);
                indent(depth); printf("}\n");
                return;
            case Boolean: printf("%s (d=%s)", tf_str[data.boolean.val], tf_str[data.boolean.default_val]); break;
            case Enum:
                printf("%s (d=%s of { ",
                    data_enum.values[data_enum.val].c_str(),
                    data_enum.values[data_enum.default_val].c_str());
                for (unsigned int i = 0; i < data_enum.values.size(); i++) {
                    if (i) printf(", ");
                    printf("%s ", data_enum.values[i].c_str());
                }
                printf("})");
                break;
            case Integer: printf("%d (d=%d)", data.integer.val, data.integer.default_val); break;
            case Number: printf("%g (d=%g)", data.number.val, data.number.default_val); break;
            case String: printf("\"%s\" (d=\"%s\")", string.val.c_str(), string.default_val.c_str()); break;
            default:
                assert(false);
                break;
        }
        printf("\n");
    }

    //
    // Update tree values from the given TOML table
    //
    void update_from_table(const toml::table &tbl, int depth = 0, std::string path = "")
    {
        for (auto&& [k, v] : tbl) {
            std::string cpath = path.length() ? path + "." + std::string(k)
                                              : std::string(k);

#if DEBUG
            fprintf(stderr, "Currently at %s\n", cpath.c_str());
#endif

            CNode *cnode = child(k.str());
            if (!cnode) {
                fprintf(stderr, "Warning: unrecognized ");
                report_key_line_col(cpath, v.source());
                fprintf(stderr, "\n");
                continue;
            }

            if (!check_type(v, cnode->type)) {
                fprintf(stderr, "Error: incorrect type for ");
                report_key_line_col(cpath, v.source());
                fprintf(stderr, "\n");
                continue;
            }

            if (v.is_table()) {
                cnode->update_from_table(*v.as_table(), depth+1, cpath);
                continue;
            }

            if (cnode->type == Boolean) {
                cnode->set_boolean_tv(*v.value<bool>(), v.source(), cpath);
            } else if (cnode->type == Enum) {
                cnode->set_enum_tv(*v.value<std::string>(), v.source(), cpath);
            } else if (cnode->type == Integer) {
                cnode->set_integer_tv(*v.value<int>(), v.source(), cpath);
            } else if (cnode->type == Number) {
                cnode->set_number_tv(*v.value<float>(), v.source(), cpath);
            } else if (cnode->type == String) {
                cnode->set_string_tv(*v.value<std::string>(), v.source(), cpath);
            } else if (v.is_array()) {
                auto arr = v.as_array();
                int len = arr->size();
                cnode->children.clear();
                if (len > 0) {
                    int i = 0;
                    for (const toml::node &elem : *arr) {
                        if (!check_type(elem, cnode->array_item_type->type)) {
                            fprintf(stderr, "Error: Unexpected array entry type at ");
                            report_line_col(elem.source());
                            fprintf(stderr, "\n");
                            continue;
                        }
                        cnode->children.push_back(*cnode->array_item_type);

                        if (cnode->array_item_type->type == Table) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "[%d]", i);
                            cnode->children.back().update_from_table(*elem.as_table(), depth+1, cpath + buf);
                            // FIXME: If the item is invalid this leaves default values initialized.
                            //        add an error flag for it
                        } else if (cnode->array_item_type->type == Boolean) {
                            cnode->children.back().set_boolean_tv(*elem.value<bool>(), elem.source(), cpath);
                        } else if (cnode->array_item_type->type == Enum) {
                            cnode->children.back().set_enum_tv(*elem.value<std::string>(), elem.source(), cpath);
                        } else if (cnode->array_item_type->type == Integer) {
                            cnode->children.back().set_integer_tv(*elem.value<int>(), elem.source(), cpath);
                        } else if (cnode->array_item_type->type == Number) {
                            cnode->children.back().set_number_tv(*elem.value<float>(), elem.source(), cpath);
                        } else if (cnode->array_item_type->type == String) {
                            cnode->children.back().set_string_tv(*elem.value<std::string>(), elem.source(), cpath);
                        } else {
                            assert(false);
                        }
                        i++;
                    }
                }
            } else {
                assert(false);
            }
        }
    }

    //
    // Check that the toml node type matches the expected CNode type
    //
    bool check_type(const toml::node &v, CNodeType expected)
    {
        switch (expected) {
        case Array:   return v.is_array();
        case Boolean: return v.is_boolean();
        case Enum:    return v.is_string();
        case Integer: return v.is_integer();
        case Number:  return v.is_number();
        case String:  return v.is_string();
        case Table:   return v.is_table();
        default: assert(false);
        }
    }

    //
    // Output "line y column z" to stderr for given input source region
    //
    void report_line_col(const toml::source_region &src) {
        fprintf(stderr, "line %d column %d", src.begin.line, src.begin.column);
    }

    //
    // Output "key 'x' at line y column z" to stderr for given input source region
    //
    void report_key_line_col(const std::string &key, const toml::source_region &src)
    {
        fprintf(stderr, "key '%s' at ", key.c_str());
        report_line_col(src);
    }

    //
    // Set boolean value
    //
    void set_boolean_tv(bool v, const toml::source_region &from, std::string path)
    {
#if DEBUG
        fprintf(stderr, "%s<%s> = %d at ", path.c_str(), type_names[type], v);
        report_line_col(from);
        fprintf(stderr, "\n");
#endif
        data.boolean.val = v;
    }

    //
    // Set enumerated type value
    //
    void set_enum_by_index(int idx)
    {
        data_enum.val = idx;
    }

    void set_enum_tv(std::string v, const toml::source_region &from, std::string path)
    {
        int idx = enum_str_to_int(v);
        if (idx < 0) {
            fprintf(stderr, "Error: invalid value for ");
            report_key_line_col(path, from);
            fprintf(stderr, "\n");
            return;
        }

#if DEBUG
        fprintf(stderr, "%s<%s> = %s at ", path.c_str(), type_names[type], v.c_str());
        report_line_col(from);
        fprintf(stderr, "\n");
#endif
        set_enum_by_index(idx);
    }

    //
    // Set integer value
    //
    void set_integer(int v)
    {
        data.integer.val = v;
    }

    void set_integer_tv(int v, const toml::source_region &from, std::string path)
    {
#if DEBUG
        fprintf(stderr, "%s<%s> = %d at ", path.c_str(), type_names[type], v);
        report_line_col(from);
        fprintf(stderr, "\n");
#endif
        set_integer(v);
    }

    //
    // Set number value
    //
    void set_number(float v)
    {
        data.number.val = v;
    }

    void set_number_tv(float v, const toml::source_region &from, std::string path)
    {
#if DEBUG
        fprintf(stderr, "%s<%s> = %g at ", path.c_str(), type_names[type], v);
        report_line_col(from);
        fprintf(stderr, "\n");
#endif
        set_number(v);
    }

    //
    // Set string value
    //
    void set_string(std::string v)
    {
        string.val = v;
    }

    void set_string_tv(std::string v, const toml::source_region &from, std::string path)
    {
#if DEBUG
        fprintf(stderr, "%s<%s> = '%s' at ", path.c_str(), type_names[type], v.c_str());
        report_line_col(from);
        fprintf(stderr, "\n");
#endif
        set_string(v);
    }

    //
    // Store values of this node and its children to the structure `s` associated with the tree
    //
    void store_to_struct(void *s)
    {
        uint8_t *p = (uint8_t *)s + serialized.offset;
#if DEBUG
        fprintf(stderr, "Storing %s to offset %d @ %p\n", name.c_str(), serialized.offset, p);
#endif

        switch (type) {
        case Array: {
            int *pc = (int *)((uint8_t *)s + serialized.count_offset);
            *pc = children.size();
            if (children.size()) {
                *(void**)p = calloc(children.size(), serialized.size);
                p = (uint8_t *) *(void**)p;
                for (unsigned int i = 0; i < children.size(); i++) {
                    children[i].store_to_struct(p);
                    p += serialized.size;
                }
            } else {
                *(void**)p = NULL;
            }
            break;
        }
        case Boolean: *(bool*)p  = data.boolean.val;           break;
        case Enum:    *(int*)p   = data_enum.val;              break;
        case Integer: *(int*)p   = data.integer.val;           break;
        case Number:  *(float*)p = data.number.val;            break;
        case String:  *(char**)p = strdup(string.val.c_str()); break;
        case Table:
            for (auto &c : children) {
                c.store_to_struct(s);
            }
            break;
        default: assert(false);
        }
    }

    //
    // Free any allocations made
    //
    void free_allocations(void *s)
    {
        uint8_t *p = (uint8_t *)s + serialized.offset;
#if DEBUG
        fprintf(stderr, "Free %s offset %d @ %p\n", name.c_str(), serialized.offset, p);
#endif

        switch (type) {
        case Array: {
            int *pc = (int *)((uint8_t *)s + serialized.count_offset);
            *pc = 0;
            if (children.size()) {
                uint8_t *p_c = (uint8_t *) *(void**)p;
                for (unsigned int i = 0; i < children.size(); i++) {
                    children[i].free_allocations(p_c);
                    p_c += serialized.size;
                }

                free(*(void**)p);
            }
            *(void**)p = NULL;
            break;
        }
        case String:
            free(*(void**)p);
            *(void**)p = NULL;
            break;
        case Table:
            for (auto &c : children) {
                c.free_allocations(s);
            }
            break;
        default: break;
        }
    }

    //
    // Update values of this node and its children from the structure `s` associated with the tree
    //
    void update_from_struct(void *s)
    {
        uint8_t *p = (uint8_t *)s + serialized.offset;
#if DEBUG
        fprintf(stderr, "Loading %s from offset %d @ %p\n", name.c_str(), serialized.offset, p);
#endif

        switch (type) {
        case Array: {
            int *pc = (int *)((uint8_t *)s + serialized.count_offset);
            children.clear();
            p = (uint8_t *) *(void**)p;
            for (int i = 0; i < *pc; i++) {
                children.push_back(*array_item_type);
                children.back().update_from_struct(p);
                p += serialized.size;
            }
            break;
        }
        case Boolean: data.boolean.val = *(bool*)p;               break;
        case Enum:    data_enum.val    = *(int*)p;                break;
        case Integer: data.integer.val = *(int*)p;                break;
        case Number:  data.number.val  = *(float*)p;              break;
        case String:  string.val       = std::string(*(char**)p); break;
        case Table:
            for (auto &c : children) {
                c.update_from_struct(s);
            }
            break;
        default: assert(false);
        }
    }

    //
    // Check if this node's value differs from its default value
    //
    // Note: Arrays of size > 0 are always considered differing
    //
    bool differs_from_default()
    {
        switch (type) {
        case Array:   return children.size() > 0;
        case Boolean: return data.boolean.val != data.boolean.default_val;
        case Enum:    return data_enum.val    != data_enum.default_val;
        case Integer: return data.integer.val != data.integer.default_val;
        case Number:  return data.number.val  != data.number.default_val;
        case String:  return string.val.compare(string.default_val);
        case Table:
            for (auto &c : children) {
                if (c.differs_from_default()) {
                    return true;
                }
            }
            break;
        default: assert(false);
        }

        return false;
    }

    //
    // Set the current value at every node as the node's default value
    //
    void set_defaults()
    {
        switch (type) {
        case Array:   // XXX: Setting array defaults not supported.
                      //      You'd need to change array_item_type.
                      break;
        case Boolean: data.boolean.default_val = data.boolean.val; break;
        case Enum:    data_enum.default_val = data_enum.val; break;
        case Integer: data.integer.default_val = data.integer.val; break;
        case Number:  data.number.default_val = data.number.val; break;
        case String:  string.default_val = string.val; break;
        case Table:   for (auto &c : children) c.set_defaults(); break;
        default: assert(false);
        }
    }

    //
    // Generate and return a TOML-formatted configuration for nodes that differ from their default value
    //
    // Note: Arrays of size > 0 are always considered differing
    //
    std::string generate_delta_toml(std::string path = "", bool inline_table = false, int depth = 0, bool root = true)
    {
        if (!differs_from_default()) {
            return "";
        }

        if (type == Table) {
            std::string s = "";
            bool printed_table_header = false;

            std::string cpath;
            if (path.length()) {
                cpath = path + "." + name;
            } else if (!root) {
                cpath = name;
            }

            if (inline_table) {
                s += "{ ";
            }

            int i = 0;
            for (auto &c : children) {
                if (c.type == Table || !c.differs_from_default()) continue;

                if (!printed_table_header && !inline_table) {
                    if (cpath.length()) {
                        s += "[" + cpath + "]\n";
                    }
                    printed_table_header = true;
                }

                if (inline_table && i++) s += ", ";
                s += c.generate_delta_toml("", inline_table, depth, false);

                if (!inline_table) {
                    s += "\n";
                }
            }

            if (printed_table_header) {
                s += "\n";
            }

            for (auto &c : children) {
                if (c.type != Table || !c.differs_from_default()) continue;
                s += c.generate_delta_toml(cpath, inline_table, depth, false);
            }

            if (inline_table) {
                s += "}";
            }

            return s;
        }

        std::string s = "";

        if (name.length()) {
            s += string_format("%s = ", name.c_str());
        }

        switch (type) {
        case Array: {
            s += "[\n";
            int i = 0;
            for (auto &c : children) {
                if (i++) {
                    s += ",\n";
                }
                for (int d = 0; d < (depth+1); d++) s += "    ";
                s += c.generate_delta_toml("", true, depth + 1, false);
            }
            s += "\n";
            for (int d = 0; d < (depth+1); d++) s += "    ";
            s += "]";
            break;
        }
        case Boolean: s += data.boolean.val ? "true" : "false"; break;
        case Enum: {
            std::ostringstream oss;
            oss << toml::value<std::string>(data_enum.values[data_enum.val]);
            s += oss.str();
            break;
        }
        case Integer: s += string_format("%d", data.integer.val); break;
        case Number:  s += string_format("%g", data.number.val); break;
        case String: {
            std::ostringstream oss;
            oss << toml::value<std::string>(string.val);
            s += oss.str();
            break;
        }
        default: assert(false);
        }

        return s;
    }
};

//
// Helpers to generate CNodes
//

CNode ctab(std::string name, std::vector<CNode> children) {
    return CNode(name, children);
}

CNode carray(size_t o, size_t oc, size_t sz, std::string name, CNode item_type) {
    CNode node(name, Array);
    node.array_item_type = std::make_unique<CNode>(item_type);
    node.serialized.offset = o;
    node.serialized.count_offset = oc;
    node.serialized.size = sz;
    return node;
}

CNode cbool(size_t o, std::string name, bool val = false) {
    CNode node(name, Boolean);
    node.data.boolean = { val, val };
    node.serialized.offset = o;
    return node;
}

CNode cenum(size_t o, std::string name, std::vector<std::string> values, std::string value) {
    CNode node(name, Enum);
    node.data_enum.values = values;
    int idx = node.enum_str_to_int(value);
    assert(idx >= 0 && "Default value invalid");
    node.data_enum.val = node.data_enum.default_val = idx;
    node.serialized.offset = o;
    return node;
}

CNode cinteger(size_t o, std::string name, int val = 0, int min = INT_MIN, int max = INT_MAX) {
    CNode node(name, Integer);
    node.data.integer = { min, max, val, val };
    node.serialized.offset = o;
    return node;
}

CNode cnumber(size_t o, std::string name, float val = 0, float min = -FLT_MAX, float max = +FLT_MAX) {
    CNode node(name, Number);
    node.data.number = { min, max, val, val };
    node.serialized.offset = o;
    return node;
}

CNode cstring(size_t o, std::string name, std::string val) {
    CNode node(name, String);
    node.string = { val, val };
    node.serialized.offset = o;
    return node;
}
