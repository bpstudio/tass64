/*
    $Id: variables.h 1093 2016-05-12 10:50:04Z soci $

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/
#ifndef VARIABLES_H
#define VARIABLES_H
#include "stdbool.h"
#include "inttypes.h"
#include "obj.h"

extern struct Type *LABEL_OBJ;

struct Namespace;

typedef struct Label {
    Obj v;
    str_t name;
    str_t cfname;

    Obj *value;
    struct file_list_s *file_list;
    struct linepos_s epoint;
    bool ref;
    bool shadowcheck;
    bool update_after;
    bool constant;
    bool owner;
    uint8_t usepass;
    uint8_t defpass;
    uint8_t strength;
} Label;

extern void labelobj_init(void);

extern void push_context(struct Namespace *);
extern bool pop_context(void);
extern void reset_context(void);
extern void get_namespaces(Mfunc *);
extern size_t context_get_bottom(void);
extern void context_set_bottom(size_t);

extern struct Namespace *current_context, *cheap_context, *root_namespace;
extern Label *find_label(const str_t *, struct Namespace **);
extern Label *find_label2(const str_t *, struct Namespace *);
extern Label *find_label3(const str_t *, struct Namespace *, uint8_t);
extern Label *find_anonlabel(int32_t);
extern Label *find_anonlabel2(int32_t, struct Namespace *);
extern Label *new_label(const str_t *, struct Namespace *, uint8_t, bool *);
extern void labelprint(void);
extern void shadow_check(struct Namespace *);
extern void destroy_variables(void);
extern void init_variables(void);
extern void destroy_lastlb(void);
extern void new_builtin(const char *, Obj *);
#endif
