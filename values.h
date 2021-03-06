/*
    $Id: values.h 1093 2016-05-12 10:50:04Z soci $

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
#ifndef VALUES_H
#define VALUES_H
#include <stdio.h>
#include "inttypes.h"

struct Type;
struct Obj;

extern struct Obj *val_alloc(struct Type *);
extern void val_destroy(struct Obj *);
extern void val_replace(struct Obj **, struct Obj *);
extern int val_print(struct Obj *, FILE *);

extern void destroy_values(void);
extern void init_values(void);
extern void garbage_collect(void);
#endif
