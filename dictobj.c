/*
    $Id: dictobj.c 1185 2016-06-25 05:14:57Z soci $

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
#include "dictobj.h"
#include <string.h>
#include "eval.h"
#include "error.h"
#include "variables.h"

#include "intobj.h"
#include "listobj.h"
#include "strobj.h"
#include "boolobj.h"
#include "operobj.h"
#include "typeobj.h"
#include "noneobj.h"

static Type obj;

Type *DICT_OBJ = &obj;

static void dict_free(struct avltree_node *aa)
{
    struct pair_s *a = avltree_container_of(aa, struct pair_s, node);
    val_destroy(a->key);
    if (a->data != NULL) val_destroy(a->data);
    free(a);
}

static void dict_free2(struct avltree_node *aa)
{
    struct pair_s *a = avltree_container_of(aa, struct pair_s, node);
    free(a);
}

static void dict_garbage1(struct avltree_node *aa)
{
    struct pair_s *a = avltree_container_of(aa, struct pair_s, node);
    a->key->refcount--;
    if (a->data != NULL) a->data->refcount--;
}

static void dict_garbage2(struct avltree_node *aa)
{
    struct pair_s *a = avltree_container_of(aa, struct pair_s, node);
    Obj *v;
    v = a->data;
    if (v != NULL) {
        if ((v->refcount & SIZE_MSB) != 0) {
            v->refcount -= SIZE_MSB - 1;
            v->obj->garbage(v, 1);
        } else v->refcount++;
    }
    v = a->key;
    if ((v->refcount & SIZE_MSB) != 0) {
        v->refcount -= SIZE_MSB - 1;
        v->obj->garbage(v, 1);
    } else v->refcount++;
}

static MUST_CHECK Obj *create(Obj *v1, linepos_t epoint) {
    switch (v1->obj->type) {
    case T_NONE:
    case T_ERROR:
    case T_DICT: return val_reference(v1);
    default: break;
    }
    err_msg_wrong_type(v1, NULL, epoint);
    return (Obj *)ref_none();
}

static void destroy(Obj *o1) {
    Dict *v1 = (Dict *)o1;
    avltree_destroy(&v1->members, dict_free);
    if (v1->def != NULL) val_destroy(v1->def);
}

static void garbage(Obj *o1, int i) {
    Dict *v1 = (Dict *)o1;
    Obj *v;
    switch (i) {
    case -1:
        avltree_destroy(&v1->members, dict_garbage1);
        v = v1->def;
        if (v != NULL) v->refcount--;
        return;
    case 0:
        avltree_destroy(&v1->members, dict_free2);
        return;
    case 1:
        avltree_destroy(&v1->members, dict_garbage2);
        v = v1->def;
        if (v == NULL) return;
        if ((v->refcount & SIZE_MSB) != 0) {
            v->refcount -= SIZE_MSB - 1;
            v->obj->garbage(v, 1);
        } else v->refcount++;
        return;
    }
}

static bool same(const Obj *o1, const Obj *o2) {
    const Dict *v1 = (const Dict *)o1, *v2 = (const Dict *)o2;
    const struct avltree_node *n;
    const struct avltree_node *n2;
    if (o2->obj != DICT_OBJ || v1->len != v2->len) return false;
    if ((v1->def == NULL) != (v2->def == NULL)) return false;
    if (v1->def != NULL && v2->def != NULL && !v1->def->obj->same(v1->def, v2->def)) return false;
    n = avltree_first(&v1->members);
    n2 = avltree_first(&v2->members);
    while (n != NULL && n2 != NULL) {
        const struct pair_s *p, *p2;
        if (pair_compare(n, n2) != 0) return false;
        p = cavltree_container_of(n, struct pair_s, node);
        p2 = cavltree_container_of(n2, struct pair_s, node);
        if ((p->data == NULL) != (p2->data == NULL)) return false;
        if (p->data != NULL && p2->data != NULL && !p->data->obj->same(p->data, p2->data)) return false;
        n = avltree_next(n);
        n2 = avltree_next(n2);
    }
    return n == n2;
}

static MUST_CHECK Obj *len(Obj *o1, linepos_t UNUSED(epoint)) {
    Dict *v1 = (Dict *)o1;
    return (Obj *)int_from_size(v1->len);
}

static MUST_CHECK Obj *repr(Obj *o1, linepos_t epoint, size_t maxsize) {
    Dict *v1 = (Dict *)o1;
    const struct pair_s *p;
    size_t i = 0, j, ln = 2, chars = 2;
    Tuple *list = NULL;
    Obj **vals;
    Obj *v;
    Str *str;
    uint8_t *s;
    unsigned int def = (v1->def != NULL) ? 1 : 0;
    if (v1->len != 0 || def != 0) {
        ln = v1->len * 2;
        if (ln < v1->len) err_msg_out_of_memory(); /* overflow */
        ln += def;
        if (ln < def) err_msg_out_of_memory(); /* overflow */
        chars = ln + 1 + def;
        if (chars < ln) err_msg_out_of_memory(); /* overflow */
        if (chars > maxsize) return NULL;
        list = new_tuple();
        list->data = vals = list_create_elements(list, ln);
        ln = chars;
        if (v1->len != 0) {
            const struct avltree_node *n = avltree_first(&v1->members);
            while (n != NULL) {
                p = cavltree_container_of(n, struct pair_s, node);
                v = p->key->obj->repr(p->key, epoint, maxsize - chars);
                if (v == NULL || v->obj != STR_OBJ) goto error;
                str = (Str *)v;
                ln += str->len;
                if (ln < str->len) err_msg_out_of_memory(); /* overflow */
                chars += str->chars;
                if (chars > maxsize) goto error2;
                vals[i++] = v;
                if (p->data != NULL) {
                    v = p->data->obj->repr(p->data, epoint, maxsize - chars);
                    if (v == NULL || v->obj != STR_OBJ) goto error;
                    str = (Str *)v;
                    ln += str->len;
                    if (ln < str->len) err_msg_out_of_memory(); /* overflow */
                    chars += str->chars;
                    if (chars > maxsize) goto error2;
                } else {
                    v = (Obj *)ref_none();
                    ln--;
                    chars--;
                }
                vals[i++] = v;
                n = avltree_next(n);
            }
        }
        if (def != 0) {
            v = v1->def->obj->repr(v1->def, epoint, maxsize - chars);
            if (v == NULL || v->obj != STR_OBJ) {
            error:
                list->len = i;
                val_destroy(&list->v);
                return v;
            }
            str = (Str *)v;
            ln += str->len;
            if (ln < str->len) err_msg_out_of_memory(); /* overflow */
            chars += str->chars;
            if (chars > maxsize) {
            error2:
                list->len = i;
                val_destroy(&list->v);
                val_destroy(v);
                return NULL;
            }
            vals[i] = v;
        }
        list->len = i + def;
    }
    str = new_str(ln);
    str->chars = chars;
    s = str->data;
    *s++ = '{';
    for (j = 0; j < i; j++) {
        Str *str2 = (Str *)vals[j];
        if (str2->v.obj != STR_OBJ) continue;
        if (j != 0) *s++ = ((j & 1) != 0) ? ':' : ',';
        if (str2->len != 0) {
            memcpy(s, str2->data, str2->len);
            s += str2->len;
        }
    }
    if (def != 0) {
        Str *str2 = (Str *)vals[j];
        if (j != 0) *s++ = ',';
        *s++ = ':';
        if (str2->len != 0) {
            memcpy(s, str2->data, str2->len);
            s += str2->len;
        }
    }
    *s = '}';
    if (list != NULL) val_destroy(&list->v);
    return &str->v;
}


static MUST_CHECK Obj *slice(Obj *o1, oper_t op, size_t indx) {
    struct pair_s pair;
    const struct avltree_node *b;
    Obj *o2 = op->v2;
    Dict *v1 = (Dict *)o1;
    Error *err;
    Funcargs *args = (Funcargs *)o2;
    linepos_t epoint2;

    if (args->len > indx + 1) {
        err_msg_argnum(args->len, 1, indx + 1, op->epoint2);
        return (Obj *)ref_none();
    }
    o2 = args->val[indx].val;
    epoint2 = &args->val[indx].epoint;

    if (o2->obj == NONE_OBJ) return val_reference(o2);

    pair.key = o2;
    err = pair.key->obj->hash(pair.key, &pair.hash, epoint2);
    if (err != NULL) return &err->v;
    b = avltree_lookup(&pair.node, &v1->members, pair_compare);
    if (b != NULL) {
        const struct pair_s *p = cavltree_container_of(b, struct pair_s, node);
        return val_reference(p->data);
    }
    if (v1->def != NULL) {
        return val_reference(v1->def);
    }
    err = new_error(ERROR_____KEY_ERROR, epoint2);
    err->u.key = val_reference(o2);
    return &err->v; 
}

static MUST_CHECK Obj *calc2(oper_t op) {
    Obj *o2 = op->v2;

    switch (o2->obj->type) {
    case T_NONE:
    case T_ERROR:
    case T_TUPLE:
    case T_LIST:
        if (op->op != &o_MEMBER && op->op != &o_X) {
            return o2->obj->rcalc2(op);
        }
    default: break;
    }
    return obj_oper_error(op);
}

static MUST_CHECK Obj *rcalc2(oper_t op) {
    Dict *v2 = (Dict *)op->v2;
    Obj *o1 = op->v1;
    if (op->op == &o_IN) {
        struct pair_s p;
        struct avltree_node *b;
        Error *err;

        p.key = o1;
        err = p.key->obj->hash(p.key, &p.hash, op->epoint);
        if (err != NULL) return &err->v;
        b = avltree_lookup(&p.node, &v2->members, pair_compare);
        return truth_reference(b != NULL);
    }
    switch (o1->obj->type) {
    case T_NONE:
    case T_ERROR:
    case T_TUPLE:
    case T_LIST:
        return o1->obj->calc2(op);
    default: break;
    }
    return obj_oper_error(op);
}

static struct oper_s pair_oper;

int pair_compare(const struct avltree_node *aa, const struct avltree_node *bb)
{
    const struct pair_s *a = cavltree_container_of(aa, struct pair_s, node);
    const struct pair_s *b = cavltree_container_of(bb, struct pair_s, node);
    Obj *result;
    int h = a->hash - b->hash;

    if (h != 0) return h;
    pair_oper.v1 = a->key;
    pair_oper.v2 = b->key;
    result = pair_oper.v1->obj->calc2(&pair_oper);
    if (result->obj == INT_OBJ) h = ((Int *)result)->len;
    else h = pair_oper.v1->obj->type - pair_oper.v2->obj->type;
    val_destroy(result);
    return h;
}

void dictobj_init(void) {
    static struct linepos_s nopoint;

    new_type(&obj, T_DICT, "dict", sizeof(Dict));
    obj_init(&obj);
    obj.create = create;
    obj.destroy = destroy;
    obj.garbage = garbage;
    obj.same = same;
    obj.len = len;
    obj.repr = repr;
    obj.calc2 = calc2;
    obj.rcalc2 = rcalc2;
    obj.slice = slice;

    pair_oper.op = &o_CMP;
    pair_oper.epoint = &nopoint;
    pair_oper.epoint2 = &nopoint;
    pair_oper.epoint3 = &nopoint;
}

void dictobj_names(void) {
    new_builtin("dict", val_reference(&DICT_OBJ->v));
}
