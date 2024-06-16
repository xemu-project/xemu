#!/usr/bin/env python3
# Copyright (c) 2022 Matt Borgerson
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
"""
Generates C/C++ code to load/store application config based on a spec file.
"""

import yaml
import json
import logging
import argparse
import os

from typing import Optional, Sequence, Mapping, Any


logging.basicConfig(level=logging.INFO)
log = logging.getLogger()


class ConfigNode:
    def __init__(self, name: str, entry: Mapping[str, Any]):
        self.name: str = name

        # Support shorthand field: type
        if isinstance(entry, str):
            entry = {
                'type': entry
            }

        self.type: str = entry.pop('type', 'table')

        # Type name aliases
        self.type = {
            'bool': 'boolean',
            'int': 'integer',
            'float': 'number',
            'str': 'string',
        }.get(self.type, self.type)

        supported_types = ('boolean', 'string', 'integer', 'number', 'enum', 'array', 'table')
        assert self.type in supported_types, \
               f'{name}: Invalid type {self.type}, expected one of {supported_types}'

        self.array_item_type: 'ConfigNode' = None
        self.children: Sequence['ConfigNode'] = []
        self.attrs: Mapping[str, Any] = {}
        self.value: Any = None

        if self.type == 'table':
            for k, v in entry.items():
                self.children.append(ConfigNode(k, v))
        else:
            self.attrs.update(entry)
            default_values = {
                'boolean': False,
                'string':  '',
                'integer': 0,
                'number':  0.0,
                'array':   [],
            }

            self.value = entry.get('default', default_values.get(self.type, None))
            if self.type == 'enum' and self.value is None:
                self.value = list(self.values)[0]
            if self.type == 'array':
                self.array_item_type = ConfigNode('', self.items)

    def __getattr__(self, name: str) -> Any:
        if name in self.attrs:
            return self.attrs[name]
        else:
            raise AttributeError(name)

    def __str__(self, depth: int = 0) -> str:
        s = ' '*depth + f'{self.name}<{self.type}>'
        if self.type in ('boolean', 'string', 'integer', 'number', 'enum'):
            s += f' = {repr(self.value)}'
        if self.type == 'enum':
            s += f" {{{', '.join(map(repr, self.values))}}}"
        if self.type == 'array':
            s += ' of {\n'
            s += self.array_item_type.__str__(depth=depth+1)
            s += ' '*depth + '}'
        s += '\n'
        for c in self.children:
            s += c.__str__(depth=depth+1)
        return s

    def enum_name(self, path: str) -> str:
        return path.replace('.', '_').upper()

    def enum_value(self, path: str, value: str) -> str:
        return self.enum_name(path + '_' + value)

    def enum_count(self, path: str) -> str:
        return self.enum_value(path, '_COUNT')

    def gen_c_enums(self, path: str = '') -> str:
        if self.name:
            cpath = (path + '.' + self.name) if path else self.name
        else:
            cpath = path
        s = ''
        if self.type == 'enum':
            s += f'enum {self.enum_name(cpath)}_ {{\n'
            for i, v in enumerate(self.values):
                s += '\t' + self.enum_value(cpath, v) + f' = {i},\n'
            s += '\t'+self.enum_count(cpath) + f' = {len(self.values)}\n'
            s += f'}};\ntypedef int {self.enum_name(cpath)};\n\n'

        for c in self.children:
            s += c.gen_c_enums(cpath)
        if self.array_item_type:
            s += self.array_item_type.gen_c_enums(cpath)
        return s

    def field_name(self, path: str) -> str:
        return path.replace('.', '_')

    def gen_c_decl(self, path: str, array=False, var='') -> str:
        if self.type == 'boolean':
            s = f'bool '
        elif self.type == 'string':
            s = f'const char *'
        elif self.type == 'integer':
            s = f'int '
        elif self.type == 'number':
            s = f'float '
        elif self.type == 'array':
            s = f'struct {self.name}_item *'
        elif self.type == 'enum':
            s = f'{self.enum_name(path + "." + self.name)} '
        else:
            assert False

        if array:
            s += '*'
        s += var if var else self.name
        return s

    def gen_c_struct(self, path: str = '', indent: int = 0, var: str = '', array: bool = False) -> str:
        if self.name:
            cpath = (path + '.' + self.name) if path else self.name
        else:
            cpath = path
        s = ''

        if self.array_item_type:
            s += self.array_item_type.gen_c_typedefs(cpath)

        if var:
            s += '  '*indent + f'struct {var} {{\n'
        else:
            s += '  '*indent + f'struct {self.name} {{\n'

        for c in self.children:
            if c.type == 'table':
                s += c.gen_c_struct(cpath, indent+1, var=c.name)
            else:
                if c.type == 'array':
                    if c.array_item_type.type == 'table':
                        s += c.array_item_type.gen_c_struct(cpath + '.' + c.name, indent=indent+1, var=c.name, array=True)
                    else:
                        s += '  '*(indent+1) + c.array_item_type.gen_c_decl(cpath, var=c.name, array=True) + ';\n'
                    s += '  '*(indent+1) + 'unsigned int ' + c.name + '_count;\n'
                else:
                    s += '  '*(indent+1) + c.gen_c_decl(cpath) + ';\n'

        if var:
            s += '  '*indent + '} ' + ('*' if array else '') + var + ';\n'
        else:
            s += '  '*indent + '};\n'

        return s


class Config:
    def __init__(self, path: str):
        doc = open(path).read()
        doc = doc.replace('\t', '  ')  ## PyYAML doesn't like tabs
        log.debug('Input config contents:\n%s', doc)
        self.root = ConfigNode('config', yaml.load(doc, yaml.Loader))

    def gen_cnode_def_file(self, path: str = '', spath: str = '', node: Optional[ConfigNode] = None, depth: int = 0) -> str:
        if node is None:
            s = 'CNode config_tree =\n'
            s += self.gen_cnode_def_file(path='', spath=self.root.name, node=self.root, depth=1)
            s = s[0:-1] + ';\n'
            return s

        if node is self.root:
            cpath = ''
        elif node.name:
            cpath = (path + '.' + node.name) if path else node.name
        else:
            cpath = path

        s = ''
        ind = ' '*depth
        nind = ' '*(depth+1)

        if node.type == 'table':
            s += ind + f'ctab("{node.name}", {{\n'
            for i, c in enumerate(node.children):
                if i > 0:
                    s = s[0:-1] + ',\n'
                s += self.gen_cnode_def_file(path=cpath, spath=spath, node=c, depth=depth+1)
            s += ind + '})\n'
        else:

            if node.name:
                offset_of_field = nind + f'offsetof(struct {spath}, {cpath}),\n'
            elif path == '':
                # Non-table array item
                offset_of_field = nind + '0, '
                nind = ''

            if node.type == 'boolean':
                s += ind + f'cbool(\n' + offset_of_field
                s += nind + f'"{node.name}", {"true" if node.value else "false"})\n'
            elif node.type == 'string':
                s += ind + f'cstring(\n' + offset_of_field
                s += nind + f'"{node.name}", "{node.value}")\n'
            elif node.type == 'integer':
                s += ind + f'cinteger(\n' + offset_of_field
                s += nind + f'"{node.name}", {node.value})\n'
            elif node.type == 'number':
                s += ind + f'cnumber(\n' + offset_of_field
                s += nind + f'"{node.name}", {node.value})\n'
            elif node.type == 'array':
                s += ind + f'carray(\n' + offset_of_field
                s += nind + f'offsetof(struct {spath}, {cpath}_count),\n'
                if node.array_item_type.type == 'table':
                    s += nind + f'sizeof(struct {spath}::{cpath.replace(".", "::")}),\n'
                else:
                    s += nind + f'sizeof(((struct {spath} *){{0}})->{cpath}[0]),\n'
                s += nind + f'"{node.name}", \n'
                spath = spath + '::' + cpath.replace('.', '::')
                s += self.gen_cnode_def_file(path='', spath=spath, node=node.array_item_type, depth=depth+1)
                s += ind + ')\n'
            elif node.type == 'enum':
                s += ind + f'cenum(\n' + offset_of_field
                assert node.value, f"Missing default value for {cpath}"
                s += nind + f'"{node.name}", {{{", ".join(json.dumps(v) for v in node.values)}}}, {json.dumps(node.value)})\n'
            else:
                assert False, node.type

        return s


    def gen_c_file(self) -> str:
        return (  '#include <stdbool.h>\n'
                + self.root.gen_c_enums()
                + self.root.gen_c_struct())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('spec', help='Input config specification path')
    ap.add_argument('output', help='Output config header path')
    args = ap.parse_args()
    c = Config(args.spec)

    s = f'''\
#ifndef CONFIG_H
#define CONFIG_H
{c.gen_c_file()}
#endif

#ifdef DEFINE_CONFIG_TREE
{c.gen_cnode_def_file()}
#endif
'''

    with open(args.output, 'w') as f:
        f.write(s)

if __name__ == '__main__':
    main()
