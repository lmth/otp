/*
 * %CopyrightBegin%
 *
 * Copyright Ericsson AB 2005-2021. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * %CopyrightEnd%
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "erl_printf_term.h"
#include "sys.h"
#include "global.h"
#include "big.h"
#include "erl_map.h"
#include "erl_binary.h"

#define PRINT_CHAR(CNT, FN, ARG, C)					\
do {									\
    int res__ = erts_printf_char((FN), (ARG), (C));			\
    if (res__ < 0)							\
	return res__;							\
    (CNT) += res__;							\
} while (0)

#define PRINT_STRING(CNT, FN, ARG, STR)					\
do {									\
    int res__ = erts_printf_string((FN), (ARG), (STR));			\
    if (res__ < 0)							\
	return res__;							\
    (CNT) += res__;							\
} while (0)

#define PRINT_BUF(CNT, FN, ARG, BUF, LEN)				\
do {									\
    int res__ = erts_printf_buf((FN), (ARG), (char*)(BUF), (LEN));	\
    if (res__ < 0)							\
	return res__;							\
    (CNT) += res__;							\
} while (0)

#define PRINT_POINTER(CNT, FN, ARG, PTR)				\
do {									\
    int res__ = erts_printf_pointer((FN), (ARG), (void *) (PTR));	\
    if (res__ < 0)							\
	return res__;							\
    (CNT) += res__;							\
} while (0)

#define PRINT_UWORD(CNT, FN, ARG, C, P, W, I)				\
do {									\
    int res__ = erts_printf_uword((FN), (ARG), (C), (P), (W), (I));	\
    if (res__ < 0)							\
	return res__;							\
    (CNT) += res__;							\
} while (0)

#define PRINT_SWORD(CNT, FN, ARG, C, P, W, I)				\
do {									\
    int res__ = erts_printf_sword((FN), (ARG), (C), (P), (W), (I));	\
    if (res__ < 0)							\
	return res__;							\
    (CNT) += res__;							\
} while (0)

#define PRINT_UWORD64(CNT, FN, ARG, C, P, W, I)				\
do {									\
    int res__ = erts_printf_uword64((FN), (ARG), (C), (P), (W), (I));	\
    if (res__ < 0)							\
	return res__;							\
    (CNT) += res__;							\
} while (0)

#define PRINT_SWORD64(CNT, FN, ARG, C, P, W, I)				\
do {									\
    int res__ = erts_printf_sword64((FN), (ARG), (C), (P), (W), (I));	\
    if (res__ < 0)							\
	return res__;							\
    (CNT) += res__;							\
} while (0)

#define PRINT_DOUBLE(CNT, FN, ARG, C, P, W, I)				\
do {									\
    int res__ = erts_printf_double((FN), (ARG), (C), (P), (W), (I));	\
    if (res__ < 0)							\
	return res__;							\
    (CNT) += res__;							\
} while (0)

#define PRINT_ATOM(CNT, FN, ARG, ATOM, DCOUNT)              \
do {                                                        \
    int res__ = print_atom_name(FN, ARG, ATOM, DCOUNT);     \
    if (res__ < 0)                                          \
        return res__;                                       \
    (CNT) += res__;                                         \
    if (*(DCOUNT) <= 0)                                     \
        goto L_done;                                        \
} while(0)


/* CTYPE macros */

#define LATIN1

#define IS_DIGIT(c)  ((c) >= '0' && (c) <= '9')
#ifdef LATIN1
#define IS_LOWER(c)  (((c) >= 'a' && (c) <= 'z') \
		      || ((c) >= 128+95 && (c) <= 255 && (c) != 247))
#define IS_UPPER(c)  (((c) >= 'A' && (c) <= 'Z') \
		      || ((c) >= 128+64 && (c) <= 128+94 && (c) != 247-32))
#else
#define IS_LOWER(c)  ((c) >= 'a' && (c) <= 'z')
#define IS_UPPER(c)  ((c) >= 'A' && (c) <= 'Z')
#endif

#define IS_ALNUM(c)  (IS_DIGIT(c) || IS_LOWER(c) || IS_UPPER(c))

/* We don't include 160 (non-breaking space). */
#define IS_SPACE(c)  (c == ' ' || c == '\n' || c == '\t' || c == '\r')

#ifdef LATIN1
#define IS_CNTRL(c)  ((c) < ' ' || (c) == 127 \
		      || ((c) >= 128 && (c) < 128+32))
#else
/* Treat all non-ASCII as control characters */
#define IS_CNTRL(c)  ((c) < ' ' || (c) >= 127)
#endif

/* return 0 if list is not a non-empty flat list of printable characters */

static int
is_printable_string(Eterm list) {
    int len = 0;
    int c;

    while(is_list(list)) {
	Eterm* consp = list_val(list);
	Eterm hd = CAR(consp);

	if (!is_byte(hd))
	    return 0;
	c = signed_val(hd);
	/* IS_PRINT || IS_SPACE would be another way to put it */
	if (IS_CNTRL(c) && !IS_SPACE(c))
	   return 0;
	len++;
	list = CDR(consp);
    }
    if (is_nil(list))
	return len;
    return 0;
}

static int is_printable_ascii(byte* bytep, Uint bytesize, Uint bitoffs)
{
    if (!bitoffs) {
	while (bytesize--) {
	    if (*bytep < ' ' || *bytep >= 127)
		return 0;
	    bytep++;
	}
    } else {
	while (bytesize--) {
	    byte octet = (bytep[0] << bitoffs) | (bytep[1] >> (8-bitoffs));
	    if (octet < ' ' || octet >= 127)
		return 0;
	    bytep++;
	}
    }
    return 1;
}

/*
 * Helper function for print_atom_name(). Not generally useful.
 */
static ERTS_INLINE int latin1_char(int c1, int c2)
{
    if ((c1 & 0x80) == 0) {
        /* Plain old 7-bit ASCII. */
        return c1;
    } else if ((c1 & 0xE0) == 0xC0) {
        /* Unicode code points from 0x80 through 0x7FF. */
        ASSERT((c2 & 0xC0) == 0x80);
        return (c1 & 0x1F) << 6 | (c2 & 0x3F);
    } else if ((c1 & 0xC0) == 0x80) {
        /* A continutation byte in a utf8 sequence. Pretend that it is
         * a character that is allowed in an atom. */
        return 'a';
    } else {
        /* The start of a utf8 sequence comprising three or four
         * bytes. Always needs quoting. */
        return 0;
    }
}

/*
 * Print a atom, quoting it if necessary.
 *
 * Atoms are encoded in utf8. Since we have full control over creation
 * of atoms, the utf8 encoding is always correct and there is no need
 * to check for errors.
 */
static int print_atom_name(fmtfn_t fn, void* arg, Eterm atom, long *dcount)
{
    int n, i;
    int res;
    int need_quote;
    int pos;
    byte *s;
    byte *cpos;
    int c;
    int lc;

    res = 0;
    i = atom_val(atom);

    if ((i < 0) || (i >= atom_table_size()) ||  (atom_tab(i) == NULL)) {
	PRINT_STRING(res, fn, arg, "<bad atom index: ");
	PRINT_SWORD(res, fn, arg, 'd', 0, 1, (ErlPfSWord) i);
	PRINT_CHAR(res, fn, arg, '>');
	return res;
    }

    s = atom_tab(i)->name;
    n = atom_tab(i)->len;

    *dcount -= atom_tab(i)->len;

    if (n == 0) {
        /* The empty atom: '' */
	PRINT_STRING(res, fn, arg, "''");
	return res;
    }

    /*
     * Find out whether the atom will need quoting. Quoting is not necessary
     * if the following applies:
     *
     *   - The first character is a lowercase letter in the Latin-1 code
     *     block (0-255).
     *
     *   - All other characters are either alphanumeric characters in
     *     the Latin-1 code block or the character '_'.
     */

    need_quote = 0;
    cpos = s;
    pos = n - 1;
    c = *cpos++;
    lc = latin1_char(c, *cpos);
    if (!IS_LOWER(lc))
	need_quote++;
    else {
	while (pos--) {
	    c = *cpos++;
            lc = latin1_char(c, *cpos);
	    if (!IS_ALNUM(lc) && lc != '_') {
		need_quote++;
		break;
	    }
	}
    }

    /*
     * Now output the atom, including single quotes if needed.
     *
     * Control characters (including the range 128-159) must
     * be specially printed. Therefore, we must do a partial
     * decoding of the utf8 encoding.
     */
    cpos = s;
    pos = n;
    if (need_quote)
	PRINT_CHAR(res, fn, arg, '\'');
    while(pos--) {
	c = *cpos++;
	switch(c) {
	case '\'': PRINT_STRING(res, fn, arg, "\\'"); break;
	case '\\': PRINT_STRING(res, fn, arg, "\\\\"); break;
	case '\n': PRINT_STRING(res, fn, arg, "\\n"); break;
	case '\f': PRINT_STRING(res, fn, arg, "\\f"); break;
	case '\t': PRINT_STRING(res, fn, arg, "\\t"); break;
	case '\r': PRINT_STRING(res, fn, arg, "\\r"); break;
	case '\b': PRINT_STRING(res, fn, arg, "\\b"); break;
	case '\v': PRINT_STRING(res, fn, arg, "\\v"); break;
	default:
            if (c < ' ') {
                /* ASCII control character (0-31). */
		PRINT_CHAR(res, fn, arg, '\\');
		PRINT_UWORD(res, fn, arg, 'o', 1, 3, (ErlPfUWord) c);
            } else if (c >= 0x80) {
                /* A multi-byte utf8-encoded code point. Determine the
                 * length of the sequence. */
                int n;
                if ((c & 0xE0) == 0xC0) {
                    n = 2;
                } else if ((c & 0xF0) == 0xE0) {
                    n = 3;
                } else {
                    ASSERT((c & 0xF8) == 0xF0);
                    n = 4;
                }
                ASSERT(pos - n + 1 >= 0);

                if (c == 0xC2 && *cpos < 0xA0) {
                    /* Extended ASCII control character (128-159). */
                    ASSERT(pos > 0);
                    ASSERT(0x80 <= *cpos);
                    PRINT_CHAR(res, fn, arg, '\\');
                    PRINT_UWORD(res, fn, arg, 'o', 1, 3, (ErlPfUWord) *cpos);
                    pos--, cpos++;
                } else {
                    PRINT_BUF(res, fn, arg, cpos-1, n);
                    cpos += n - 1;
                    pos -= n - 1;
                }
            } else {
                /* Printable ASCII character. */
		PRINT_CHAR(res, fn, arg, (char) c);
            }
	    break;
	}
    }
    if (need_quote)
	PRINT_CHAR(res, fn, arg, '\'');
    return res;
}

#define PRT_BAR                ((Eterm) 0)
#define PRT_COMMA              ((Eterm) 1)
#define PRT_CLOSE_LIST         ((Eterm) 2)
#define PRT_CLOSE_TUPLE        ((Eterm) 3)
#define PRT_ASSOC              ((Eterm) 4)
#define PRT_TERM               ((Eterm) 5)
#define PRT_ONE_CONS           ((Eterm) 6)
#define PRT_PATCH_FUN_SIZE     ((Eterm) 7)
#define PRT_LAST_ARRAY_ELEMENT ((Eterm) 8) /* Note! Must be last... */

#if 0
static char *format_binary(Uint16 x, char *b) {
    int z;
    b[16] = '\0';
    for (z = 0; z < 16; z++) { 
	b[15-z] = ((x>>z) & 0x1) ? '1' : '0'; 
    }
    return b;
}
#endif

static int
print_term(fmtfn_t fn, void* arg, Eterm obj, long *dcount) {
    DECLARE_WSTACK(s);
    int res;
    int i;
    Eterm val;
    Uint32 *ref_num;
    union {
	UWord word;
	Eterm* ptr;
    }popped;
    Eterm* nobj;
    Wterm wobj;

    res = 0;

    goto L_jump_start;

 L_outer_loop:
    while (!WSTACK_ISEMPTY(s)) {
	switch (val = WSTACK_POP(s)) {
	case PRT_COMMA:
	    PRINT_CHAR(res, fn, arg, ',');
	    goto L_outer_loop;
	case PRT_BAR:
	    PRINT_CHAR(res, fn, arg, '|');
	    goto L_outer_loop;
	case PRT_CLOSE_LIST:
	    PRINT_CHAR(res, fn, arg, ']');
	    goto L_outer_loop;
	case PRT_CLOSE_TUPLE:
	    PRINT_CHAR(res, fn, arg, '}');
	    goto L_outer_loop;
	case PRT_ASSOC:
	    PRINT_STRING(res, fn, arg, "=>");
	    goto L_outer_loop;
	default:
	    popped.word = WSTACK_POP(s);

	    switch (val) {
	    case PRT_TERM:
		obj = (Eterm) popped.word;
		break;
	    case PRT_ONE_CONS:
		obj = (Eterm) popped.word;
	    L_print_one_cons:
		{
		    Eterm* cons = list_val(obj);
		    Eterm tl;
		    
		    obj = CAR(cons);
		    tl = CDR(cons);
		    if (is_not_nil(tl)) {
			if (is_list(tl)) {
			    WSTACK_PUSH3(s, tl, PRT_ONE_CONS, PRT_COMMA);
			} else {
			    WSTACK_PUSH3(s, tl, PRT_TERM, PRT_BAR);
			}
		    }
		}
		break;
	    case PRT_LAST_ARRAY_ELEMENT:
		obj = *popped.ptr;
		break;
	    default:		/* PRT_LAST_ARRAY_ELEMENT+1 and upwards */
		obj = *popped.ptr;
	        WSTACK_PUSH3(s, (UWord) (popped.ptr + 1), val-1, PRT_COMMA);
		break;
	    }
	    break;
	}

    L_jump_start:

	if ((*dcount)-- <= 0)
	    goto L_done;

	if (is_non_value(obj)) {
            PRINT_STRING(res, fn, arg, "<TNV>");
	    goto L_done;
        } else if (is_CP(obj)) {
            const ErtsCodeMFA* mfa = erts_find_function_from_pc(cp_val(obj));
            if (mfa) {
                const UWord *func_start = erts_codemfa_to_code(mfa);
                const UWord *cp_addr = (UWord*)cp_val(obj);

                PRINT_STRING(res, fn, arg, "<");
                PRINT_ATOM(res, fn, arg, mfa->module, dcount);
                PRINT_STRING(res, fn, arg, ":");
                PRINT_ATOM(res, fn, arg, mfa->function, dcount);
                PRINT_STRING(res, fn, arg, "/");
                PRINT_UWORD(res, fn, arg, 'u', 0, 1, (ErlPfUWord) mfa->arity);
                PRINT_STRING(res, fn, arg, "+");
                PRINT_UWORD(res, fn, arg, 'u', 0, 1, (ErlPfUWord) (cp_addr - func_start));
                PRINT_STRING(res, fn, arg, ">");
            } else {
                PRINT_STRING(res, fn, arg, "<cp/header:");
                PRINT_POINTER(res, fn, arg, cp_val(obj));
                PRINT_CHAR(res, fn, arg, '>');
            }
	    goto L_done;
	}
	wobj = (Wterm)obj;
	switch (tag_val_def(wobj)) {
	case NIL_DEF:
	    PRINT_STRING(res, fn, arg, "[]");
	    break;
	case ATOM_DEF: {
            PRINT_ATOM(res, fn, arg, obj, dcount);
	    break;
	}
	case SMALL_DEF:
	    PRINT_SWORD(res, fn, arg, 'd', 0, 1, (ErlPfSWord) signed_val(obj));
	    break;
	case BIG_DEF: {
	    int print_res;
	    char def_buf[64];
	    char *buf, *big_str;
	    Uint sz = (Uint) big_integer_estimate(wobj, 10);
	    sz++;
	    if (sz <= 64)
		buf = &def_buf[0];
	    else
		buf = erts_alloc(ERTS_ALC_T_TMP, sz);
	    big_str = erts_big_to_string(wobj, 10, buf, sz);
	    print_res = erts_printf_string(fn, arg, big_str);
	    if (buf != &def_buf[0])
		erts_free(ERTS_ALC_T_TMP, (void *) buf);
	    if (print_res < 0) {
		res = print_res;
		goto L_done;
	    }
	    res += print_res;
	    break;
	}
	case REF_DEF:
            if (!ERTS_IS_CRASH_DUMPING)
                erts_magic_ref_save_bin(obj);
            /* fall through... */
	case EXTERNAL_REF_DEF:
	    PRINT_STRING(res, fn, arg, "#Ref<");
	    PRINT_UWORD(res, fn, arg, 'u', 0, 1,
			(ErlPfUWord) ref_channel_no(wobj));
	    ref_num = ref_numbers(wobj);
	    for (i = ref_no_numbers(wobj)-1; i >= 0; i--) {
		PRINT_CHAR(res, fn, arg, '.');
		PRINT_UWORD(res, fn, arg, 'u', 0, 1, (ErlPfUWord) ref_num[i]);
	    }
	    PRINT_CHAR(res, fn, arg, '>');
	    break;
	case PID_DEF:
	case EXTERNAL_PID_DEF:
	    PRINT_CHAR(res, fn, arg, '<');
	    PRINT_UWORD(res, fn, arg, 'u', 0, 1,
			(ErlPfUWord) pid_channel_no(wobj));
	    PRINT_CHAR(res, fn, arg, '.');
	    PRINT_UWORD(res, fn, arg, 'u', 0, 1,
			(ErlPfUWord) pid_number(wobj));
	    PRINT_CHAR(res, fn, arg, '.');
	    PRINT_UWORD(res, fn, arg, 'u', 0, 1,
			(ErlPfUWord) pid_serial(wobj));
	    PRINT_CHAR(res, fn, arg, '>');
	    break;
	case PORT_DEF:
	case EXTERNAL_PORT_DEF:
	    PRINT_STRING(res, fn, arg, "#Port<");
	    PRINT_UWORD(res, fn, arg, 'u', 0, 1,
			(ErlPfUWord) port_channel_no(wobj));
	    PRINT_CHAR(res, fn, arg, '.');
	    PRINT_UWORD64(res, fn, arg, 'u', 0, 1,
			  (ErlPfUWord64) port_number(wobj));
	    PRINT_CHAR(res, fn, arg, '>');
	    break;
	case LIST_DEF:
	    if (is_printable_string(obj)) {
		int c;
		PRINT_CHAR(res, fn, arg, '"');
		nobj = list_val(obj);
		while (1) {
		    if ((*dcount)-- <= 0)
			goto L_done;
		    c = signed_val(*nobj++);
		    if (c == '\n')
			PRINT_STRING(res, fn, arg, "\\n");
		    else {
			if (c == '"')
			    PRINT_CHAR(res, fn, arg, '\\');
			PRINT_CHAR(res, fn, arg, (char) c);
		    }
		    if (is_not_list(*nobj))
			break;
		    nobj = list_val(*nobj);
		}
		PRINT_CHAR(res, fn, arg, '"');
	    } else {
		PRINT_CHAR(res, fn, arg, '[');
		WSTACK_PUSH(s,PRT_CLOSE_LIST);
		goto L_print_one_cons;
	    }
	    break;
	case TUPLE_DEF:
	    nobj = tuple_val(wobj);	/* pointer to arity */
	    i = arityval(*nobj);	/* arity */
	    PRINT_CHAR(res, fn, arg, '{');
	    WSTACK_PUSH(s,PRT_CLOSE_TUPLE);
	    ++nobj;
	    if (i > 0) {
		WSTACK_PUSH2(s, (UWord) nobj, PRT_LAST_ARRAY_ELEMENT+i-1);
	    }
	    break;
	case FLOAT_DEF: {
	    FloatDef ff;
	    GET_DOUBLE(wobj, ff);
	    PRINT_DOUBLE(res, fn, arg, 'e', 6, 0, ff.fd);
	}
	    break;
	case BINARY_DEF:
	    {
		byte* bytep;
		Uint bytesize = binary_size(obj);
		Uint bitoffs;
		Uint bitsize;
		byte octet;
		ERTS_GET_BINARY_BYTES(obj, bytep, bitoffs, bitsize);

		if (bitsize || !bytesize
		    || !is_printable_ascii(bytep, bytesize, bitoffs)) {
		    int is_first = 1;
		    PRINT_STRING(res, fn, arg, "<<");
		    while (bytesize) {
			if (is_first)
			    is_first = 0;
			else
			    PRINT_CHAR(res, fn, arg, ',');
			if (bitoffs)
			    octet = (bytep[0] << bitoffs) | (bytep[1] >> (8-bitoffs));
			else
			    octet = bytep[0];
			PRINT_UWORD(res, fn, arg, 'u', 0, 1, octet);
			++bytep;
			--bytesize;
                        if ((*dcount)-- <= 0)
                            goto L_done;
		    }
		    if (bitsize) {
			Uint bits = bitoffs + bitsize;
			octet = bytep[0];
			if (bits < 8)
			    octet >>= 8 - bits;
			else if (bits > 8) {
			    bits -= 8;  /* bits in last byte */
			    octet <<= bits;
			    octet |= bytep[1] >> (8 - bits);
			}
			octet &= (1 << bitsize) - 1;
			if (is_first)
			    is_first = 0;
			else
			    PRINT_CHAR(res, fn, arg, ',');
			PRINT_UWORD(res, fn, arg, 'u', 0, 1, octet);
			PRINT_CHAR(res, fn, arg, ':');
			PRINT_UWORD(res, fn, arg, 'u', 0, 1, bitsize);
		    }
		    PRINT_STRING(res, fn, arg, ">>");
		}
		else {
		    PRINT_STRING(res, fn, arg, "<<\"");
		    while (bytesize) {
			if (bitoffs)
			    octet = (bytep[0] << bitoffs) | (bytep[1] >> (8-bitoffs));
			else
			    octet = bytep[0];
			if (octet == '"')
			    PRINT_CHAR(res, fn, arg, '\\');
			PRINT_CHAR(res, fn, arg, octet);
			++bytep;
			--bytesize;
                        if ((*dcount)-- <= 0)
                            goto L_done;
		    }
		    PRINT_STRING(res, fn, arg, "\">>");
		}
	    }
	    break;
	case EXPORT_DEF:
	    {
		Export* ep = *((Export **) (export_val(wobj) + 1));
		long tdcount;
		int tres;

		PRINT_STRING(res, fn, arg, "fun ");

		/* We pass a temporary 'dcount' and adjust the real one later to ensure
		 * that the fun doesn't get split up between the module and function
		 * name. */
		tdcount = MAX_ATOM_SZ_LIMIT;
		tres = print_atom_name(fn, arg, ep->info.mfa.module, &tdcount);
		if (tres < 0) {
		    res = tres;
		    goto L_done;
		}
		*dcount -= (MAX_ATOM_SZ_LIMIT - tdcount);
		res += tres;

		PRINT_CHAR(res, fn, arg, ':');

		tdcount = MAX_ATOM_SZ_LIMIT;
		tres = print_atom_name(fn, arg, ep->info.mfa.function, &tdcount);
		if (tres < 0) {
		    res = tres;
		    goto L_done;
		}
		*dcount -= (MAX_ATOM_SZ_LIMIT - tdcount);
		res += tres;

		PRINT_CHAR(res, fn, arg, '/');
		PRINT_SWORD(res, fn, arg, 'd', 0, 1,
			    (ErlPfSWord) ep->info.mfa.arity);
	    }
	    break;
	case FUN_DEF:
	    {
		ErlFunThing *funp = (ErlFunThing *) fun_val(wobj);
		Atom *ap = atom_tab(atom_val(funp->fe->module));

		PRINT_STRING(res, fn, arg, "#Fun<");
		PRINT_BUF(res, fn, arg, ap->name, ap->len);
		PRINT_CHAR(res, fn, arg, '.');
		PRINT_SWORD(res, fn, arg, 'd', 0, 1,
			    (ErlPfSWord) funp->fe->old_index);
		PRINT_CHAR(res, fn, arg, '.');
		PRINT_SWORD(res, fn, arg, 'd', 0, 1,
			    (ErlPfSWord) funp->fe->old_uniq);
		PRINT_CHAR(res, fn, arg, '>');
	    }
	    break;
	case MAP_DEF: {
            Eterm *head = boxed_val(wobj);

            if (is_flatmap_header(*head)) {
                Uint n;
                Eterm *ks, *vs;
                n  = flatmap_get_size(head);
                ks = flatmap_get_keys(head);
                vs = flatmap_get_values(head);

                PRINT_CHAR(res, fn, arg, '#');
                PRINT_CHAR(res, fn, arg, '{');
                WSTACK_PUSH(s, PRT_CLOSE_TUPLE);
                if (n > 0) {
                    n--;
                    WSTACK_PUSH5(s, vs[n], PRT_TERM, PRT_ASSOC, ks[n], PRT_TERM);
                    while (n--) {
                        WSTACK_PUSH6(s, PRT_COMMA, vs[n], PRT_TERM, PRT_ASSOC,
                                ks[n], PRT_TERM);
                    }
                }
            } else {
                Uint n, mapval;
                mapval = MAP_HEADER_VAL(*head);
                switch (MAP_HEADER_TYPE(*head)) {
                case MAP_HEADER_TAG_HAMT_HEAD_ARRAY:
                case MAP_HEADER_TAG_HAMT_HEAD_BITMAP:
                    PRINT_STRING(res, fn, arg, "#<");
                    PRINT_UWORD(res, fn, arg, 'x', 0, 1, mapval);
                    PRINT_STRING(res, fn, arg, ">{");
                    WSTACK_PUSH(s,PRT_CLOSE_TUPLE);
                    n = hashmap_bitcount(mapval);
                    ASSERT(n < 17);
                    head += 2;
                    if (n > 0) {
                        n--;
                        WSTACK_PUSH(s, head[n]);
                        WSTACK_PUSH(s, PRT_TERM);
                        while (n--) {
                            WSTACK_PUSH(s, PRT_COMMA);
                            WSTACK_PUSH(s, head[n]);
                            WSTACK_PUSH(s, PRT_TERM);
                        }
                    }
                    break;
                case MAP_HEADER_TAG_HAMT_NODE_BITMAP:
                    n = hashmap_bitcount(mapval);
                    head++;
                    PRINT_CHAR(res, fn, arg, '<');
                    PRINT_UWORD(res, fn, arg, 'x', 0, 1, mapval);
                    PRINT_STRING(res, fn, arg, ">{");
                    WSTACK_PUSH(s,PRT_CLOSE_TUPLE);
                    ASSERT(n < 17);
                    if (n > 0) {
                        n--;
                        WSTACK_PUSH(s, head[n]);
                        WSTACK_PUSH(s, PRT_TERM);
                        while (n--) {
                            WSTACK_PUSH(s, PRT_COMMA);
                            WSTACK_PUSH(s, head[n]);
                            WSTACK_PUSH(s, PRT_TERM);
                        }
                    }
                    break;
                }
            }
            break;
        }
	case MATCHSTATE_DEF:
	    PRINT_STRING(res, fn, arg, "#MatchState");
	    break;
        default:
	    PRINT_STRING(res, fn, arg, "<unknown:");
	    PRINT_POINTER(res, fn, arg, wobj);
	    PRINT_CHAR(res, fn, arg, '>');
	    break;
	}
    }

 L_done:
    DESTROY_WSTACK(s);
    return res;
}


int
erts_printf_term(fmtfn_t fn, void* arg, ErlPfEterm term, long precision) {
    int res;
    ERTS_CT_ASSERT(sizeof(ErlPfEterm) == sizeof(Eterm));

    res = print_term(fn, arg, (Eterm)term, &precision);
    if (res < 0)
	return res;
    if (precision <= 0)
	PRINT_STRING(res, fn, arg, "... ");
    return res;
}

void
erts_print_term_cursor_init(ErtsPrintTermCursor *cur, Eterm root)
{
    WSTACK_INIT(&cur->wstack, ERTS_ALC_T_ESTACK);
    cur->obj = root;
    cur->sub = PRINT_TERM_CURSOR_NORMAL;
    cur->str_nobj = NULL;
    cur->bin_bytep = NULL;
    cur->bin_bytesize = 0;
    cur->bin_bitoffs = 0;
    cur->bin_bitsize = 0;
    cur->bin_is_first = 0;
    cur->big_str = NULL;
    cur->big_pos = 0;
    cur->big_len = 0;
}

void
erts_print_term_cursor_destroy(ErtsPrintTermCursor *cur)
{
    if (cur->sub != PRINT_TERM_CURSOR_DONE) {
        WSTACK_DESTROY(cur->wstack.ws);
        if (cur->big_str) {
            erts_free(ERTS_ALC_T_TMP, cur->big_str);
            cur->big_str = NULL;
        }
        cur->sub = PRINT_TERM_CURSOR_DONE;
    }
}

int
erts_print_term_step(fmtfn_t fn, void *arg,
                     ErtsPrintTermCursor *cur, long max_bytes)
{
#define ws (*(ErtsWStack *) &cur->wstack.ws)
#define STEP_FAIL() do { erts_print_term_cursor_destroy(cur); return 0; } while (0)
#define STEP_PRINT_CHAR(C)                                                    do {                                                                          int res__ = erts_printf_char(fn, arg, (C));                               if (res__ < 0)                                                                STEP_FAIL();                                                          res += res__;                                                         } while (0)
#define STEP_PRINT_STRING(STR)                                                do {                                                                          int res__ = erts_printf_string(fn, arg, (STR));                           if (res__ < 0)                                                                STEP_FAIL();                                                          res += res__;                                                         } while (0)
#define STEP_PRINT_BUF(BUF, LEN)                                              do {                                                                          int res__ = erts_printf_buf(fn, arg, (char *) (BUF), (LEN));              if (res__ < 0)                                                                STEP_FAIL();                                                          res += res__;                                                         } while (0)
#define STEP_PRINT_POINTER(PTR)                                               do {                                                                          int res__ = erts_printf_pointer(fn, arg, (void *) (PTR));                 if (res__ < 0)                                                                STEP_FAIL();                                                          res += res__;                                                         } while (0)
#define STEP_PRINT_UWORD(C, P, W, I)                                          do {                                                                          int res__ = erts_printf_uword(fn, arg, (C), (P), (W), (I));               if (res__ < 0)                                                                STEP_FAIL();                                                          res += res__;                                                         } while (0)
#define STEP_PRINT_SWORD(C, P, W, I)                                          do {                                                                          int res__ = erts_printf_sword(fn, arg, (C), (P), (W), (I));               if (res__ < 0)                                                                STEP_FAIL();                                                          res += res__;                                                         } while (0)
#define STEP_PRINT_UWORD64(C, P, W, I)                                        do {                                                                          int res__ = erts_printf_uword64(fn, arg, (C), (P), (W), (I));             if (res__ < 0)                                                                STEP_FAIL();                                                          res += res__;                                                         } while (0)
#define STEP_PRINT_DOUBLE(C, P, W, I)                                         do {                                                                          int res__ = erts_printf_double(fn, arg, (C), (P), (W), (I));              if (res__ < 0)                                                                STEP_FAIL();                                                          res += res__;                                                         } while (0)
#define STEP_PRINT_ATOM(ATOM, DCOUNT)                                         do {                                                                          int res__ = print_atom_name(fn, arg, (ATOM), (DCOUNT));                   if (res__ < 0)                                                                STEP_FAIL();                                                          res += res__;                                                         } while (0)
    int res, i, c, tres, is_first;
    long dcount, tdcount;
    Uint32 *ref_num;
    union {
        UWord word;
        Eterm *ptr;
    } popped;
    Eterm obj, val, wobj, key;
    Eterm *nobj, *head, *ks, *vs, *assoc, *tpl;
    Uint n, mapval, bitoffs, bitsize, bytesize, size, offset, sz;
    byte *bytep, octet;
    char *buf;
    ErlFunThing *funp;
    const ErlFunEntry *fe;
    const Export *ep;
    Atom *ap;
    ErtsRecordInstance *instance;
    ErtsRecordDefinition *defp;
    FloatDef ff;

    if (cur->sub == PRINT_TERM_CURSOR_DONE)
        return 0;

    res = 0;
    dcount = LONG_MAX;

    switch (cur->sub) {
    case PRINT_TERM_CURSOR_NORMAL:
        obj = cur->obj;
        break;
    case PRINT_TERM_CURSOR_STRING:
        nobj = cur->str_nobj;
        goto L_resume_string;
    case PRINT_TERM_CURSOR_BINARY:
        bytep = cur->bin_bytep;
        bytesize = cur->bin_bytesize;
        bitoffs = cur->bin_bitoffs;
        bitsize = cur->bin_bitsize;
        is_first = cur->bin_is_first;
        goto L_resume_binary;
    case PRINT_TERM_CURSOR_PRINTBIN:
        bytep = cur->bin_bytep;
        bytesize = cur->bin_bytesize;
        bitoffs = cur->bin_bitoffs;
        bitsize = cur->bin_bitsize;
        goto L_resume_printbin;
    case PRINT_TERM_CURSOR_BIGNUM:
        goto L_resume_bignum;
    default:
        ASSERT(0);
        return 0;
    }

    goto L_jump_start;

 L_outer_loop:
    while (!WSTACK_ISEMPTY(ws)) {
        switch (val = WSTACK_POP(ws)) {
        case PRT_COMMA:
            STEP_PRINT_CHAR(',');
            goto L_outer_loop;
        case PRT_BAR:
            STEP_PRINT_CHAR('|');
            goto L_outer_loop;
        case PRT_CLOSE_LIST:
            STEP_PRINT_CHAR(']');
            goto L_outer_loop;
        case PRT_CLOSE_TUPLE:
            STEP_PRINT_CHAR('}');
            goto L_outer_loop;
        case PRT_ASSOC:
            STEP_PRINT_STRING("=>");
            goto L_outer_loop;
        case PRT_EQUALS:
            STEP_PRINT_STRING("=");
            goto L_outer_loop;
        default:
            popped.word = WSTACK_POP(ws);

            switch (val) {
            case PRT_TERM:
                obj = (Eterm) popped.word;
                break;
            case PRT_ONE_CONS: {
                Eterm *cons;
                Eterm tl;

                obj = (Eterm) popped.word;
                cons = list_val(obj);
                obj = CAR(cons);
                tl = CDR(cons);
                if (is_not_nil(tl)) {
                    if (is_list(tl)) {
                        WSTACK_PUSH3(ws, tl, PRT_ONE_CONS, PRT_COMMA);
                    } else {
                        WSTACK_PUSH3(ws, tl, PRT_TERM, PRT_BAR);
                    }
                }
                break;
            }
            case PRT_LAST_ARRAY_ELEMENT:
                obj = *popped.ptr;
                break;
            default:
                obj = *popped.ptr;
                WSTACK_PUSH3(ws, (UWord) (popped.ptr + 1), val - 1, PRT_COMMA);
                break;
            }
            break;
        }

    L_jump_start:
        if (res >= max_bytes) {
            cur->obj = obj;
            cur->sub = PRINT_TERM_CURSOR_NORMAL;
            return 1;
        }

        if (is_non_value(obj)) {
            STEP_PRINT_STRING("<TNV>");
            goto L_done;
        } else if (is_CP(obj)) {
            const ErtsCodeMFA *mfa = erts_find_function_from_pc(cp_val(obj));

            if (mfa) {
                const UWord *func_start = erts_codemfa_to_code(mfa);
                const UWord *cp_addr = (UWord *) cp_val(obj);

                STEP_PRINT_STRING("<");
                STEP_PRINT_ATOM(mfa->module, &dcount);
                STEP_PRINT_STRING(":");
                STEP_PRINT_ATOM(mfa->function, &dcount);
                STEP_PRINT_STRING("/");
                STEP_PRINT_UWORD('u', 0, 1, (ErlPfUWord) mfa->arity);
                STEP_PRINT_STRING("+");
                STEP_PRINT_UWORD('u', 0, 1,
                                 (ErlPfUWord) (cp_addr - func_start));
                STEP_PRINT_STRING(">");
            } else {
                STEP_PRINT_STRING("<cp/header:");
                STEP_PRINT_POINTER(cp_val(obj));
                STEP_PRINT_CHAR('>');
            }
            goto L_done;
        } else if (is_catch(obj) && catch_val(obj) == 0) {
            STEP_PRINT_STRING("<novalue>");
            goto L_outer_loop;
        }

        switch (tag_val_def(wobj)) {
        case NIL_DEF:
            STEP_PRINT_STRING("[]");
            break;
        case ATOM_DEF:
            STEP_PRINT_ATOM(obj, &dcount);
            break;
        case SMALL_DEF:
            STEP_PRINT_SWORD('d', 0, 1, (ErlPfSWord) signed_val(obj));
            break;
        case BIG_DEF:
            if (!cur->big_str) {
                sz = (Uint) big_integer_estimate(wobj, 10);
                sz++;
                buf = erts_alloc(ERTS_ALC_T_TMP, sz);
                cur->big_str = erts_big_to_string(wobj, 10, buf, sz);
                ASSERT(cur->big_str == buf);
                cur->big_pos = 0;
                cur->big_len = sys_strlen(cur->big_str);
            }
        L_resume_bignum:
            while (cur->big_pos < cur->big_len) {
                if (res >= max_bytes) {
                    cur->sub = PRINT_TERM_CURSOR_BIGNUM;
                    return 1;
                }
                STEP_PRINT_CHAR(cur->big_str[cur->big_pos]);
                cur->big_pos++;
            }
            erts_free(ERTS_ALC_T_TMP, cur->big_str);
            cur->big_str = NULL;
            cur->big_pos = 0;
            cur->big_len = 0;
            cur->sub = PRINT_TERM_CURSOR_NORMAL;
            break;
        case REF_DEF:
            if (!ERTS_IS_CRASH_DUMPING)
                erts_magic_ref_save_bin(obj);
            ERTS_FALLTHROUGH();
        case EXTERNAL_REF_DEF:
            STEP_PRINT_STRING("#Ref<");
            STEP_PRINT_UWORD('u', 0, 1, (ErlPfUWord) ref_channel_no(wobj));
            ref_num = ref_numbers(wobj);
            for (i = ref_no_numbers(wobj) - 1; i >= 0; i--) {
                STEP_PRINT_CHAR('.');
                STEP_PRINT_UWORD('u', 0, 1, (ErlPfUWord) ref_num[i]);
            }
            STEP_PRINT_CHAR('>');
            break;
        case PID_DEF:
        case EXTERNAL_PID_DEF:
            STEP_PRINT_CHAR('<');
            STEP_PRINT_UWORD('u', 0, 1, (ErlPfUWord) pid_channel_no(wobj));
            STEP_PRINT_CHAR('.');
            STEP_PRINT_UWORD('u', 0, 1, (ErlPfUWord) pid_number(wobj));
            STEP_PRINT_CHAR('.');
            STEP_PRINT_UWORD('u', 0, 1, (ErlPfUWord) pid_serial(wobj));
            STEP_PRINT_CHAR('>');
            break;
        case PORT_DEF:
        case EXTERNAL_PORT_DEF:
            STEP_PRINT_STRING("#Port<");
            STEP_PRINT_UWORD('u', 0, 1, (ErlPfUWord) port_channel_no(wobj));
            STEP_PRINT_CHAR('.');
            STEP_PRINT_UWORD64('u', 0, 1, (ErlPfUWord64) port_number(wobj));
            STEP_PRINT_CHAR('>');
            break;
        case LIST_DEF:
            if (is_printable_string(obj)) {
                STEP_PRINT_CHAR('"');
                nobj = list_val(obj);
            L_resume_string:
                while (1) {
                    if (res >= max_bytes) {
                        cur->str_nobj = nobj;
                        cur->sub = PRINT_TERM_CURSOR_STRING;
                        return 1;
                    }
                    c = signed_val(*nobj++);
                    if (c == '\n') {
                        STEP_PRINT_STRING("\\n");
                    } else {
                        if (c == '"')
                            STEP_PRINT_CHAR('\\');
                        STEP_PRINT_CHAR((char) c);
                    }
                    if (is_not_list(*nobj))
                        break;
                    nobj = list_val(*nobj);
                }
                STEP_PRINT_CHAR('"');
                cur->sub = PRINT_TERM_CURSOR_NORMAL;
            } else {
                STEP_PRINT_CHAR('[');
                WSTACK_PUSH(ws, PRT_CLOSE_LIST);
                {
                    Eterm *cons = list_val(obj);
                    Eterm tl;

                    obj = CAR(cons);
                    tl = CDR(cons);
                    if (is_not_nil(tl)) {
                        if (is_list(tl)) {
                            WSTACK_PUSH3(ws, tl, PRT_ONE_CONS, PRT_COMMA);
                        } else {
                            WSTACK_PUSH3(ws, tl, PRT_TERM, PRT_BAR);
                        }
                    }
                }
            }
            break;
        case RECORD_DEF:
            instance = RECORD_INST_P(wobj);
            n = RECORD_INST_FIELD_COUNT(instance);
            defp = RECORD_DEF_P(instance);
            ks = defp->keys;
            vs = instance->values;
            STEP_PRINT_CHAR('#');
            STEP_PRINT_ATOM(defp->module, &dcount);
            STEP_PRINT_CHAR(':');
            STEP_PRINT_ATOM(defp->name, &dcount);
            STEP_PRINT_CHAR('{');
            WSTACK_PUSH(ws, PRT_CLOSE_TUPLE);
            if (n > 0) {
                n--;
                WSTACK_PUSH5(ws, vs[n], PRT_TERM, PRT_EQUALS, ks[n], PRT_TERM);
                while (n--) {
                    WSTACK_PUSH6(ws, PRT_COMMA, vs[n], PRT_TERM, PRT_EQUALS,
                                 ks[n], PRT_TERM);
                }
            }
            break;
        case TUPLE_DEF:
            nobj = tuple_val(wobj);
            i = arityval(*nobj);
            STEP_PRINT_CHAR('{');
            WSTACK_PUSH(ws, PRT_CLOSE_TUPLE);
            ++nobj;
            if (i > 0)
                WSTACK_PUSH2(ws, (UWord) nobj, PRT_LAST_ARRAY_ELEMENT + i - 1);
            break;
        case FLOAT_DEF:
            GET_DOUBLE(wobj, ff);
            STEP_PRINT_DOUBLE('e', 6, 0, ff.fd);
            break;
        case BITSTRING_DEF:
            ERTS_GET_BITSTRING(obj, bytep, offset, size);
            bytep += BYTE_OFFSET(offset);
            bitoffs = BIT_OFFSET(offset);
            bytesize = BYTE_SIZE(size);
            bitsize = TAIL_BITS(size);

            if (bitsize || !bytesize || !is_printable_ascii(bytep, bytesize, bitoffs)) {
                is_first = 1;
                STEP_PRINT_STRING("<<");
            L_resume_binary:
                while (bytesize) {
                    if (res >= max_bytes) {
                        cur->bin_bytep = bytep;
                        cur->bin_bytesize = bytesize;
                        cur->bin_bitoffs = bitoffs;
                        cur->bin_bitsize = bitsize;
                        cur->bin_is_first = is_first;
                        cur->sub = PRINT_TERM_CURSOR_BINARY;
                        return 1;
                    }
                    if (is_first)
                        is_first = 0;
                    else
                        STEP_PRINT_CHAR(',');
                    if (bitoffs)
                        octet = (bytep[0] << bitoffs) | (bytep[1] >> (8 - bitoffs));
                    else
                        octet = bytep[0];
                    STEP_PRINT_UWORD('u', 0, 1, octet);
                    ++bytep;
                    --bytesize;
                }
                if (bitsize) {
                    Uint bits = bitoffs + bitsize;

                    octet = bytep[0];
                    if (bits < 8)
                        octet >>= 8 - bits;
                    else if (bits > 8) {
                        bits -= 8;
                        octet <<= bits;
                        octet |= bytep[1] >> (8 - bits);
                    }
                    octet &= (1 << bitsize) - 1;
                    if (is_first)
                        is_first = 0;
                    else
                        STEP_PRINT_CHAR(',');
                    STEP_PRINT_UWORD('u', 0, 1, octet);
                    STEP_PRINT_CHAR(':');
                    STEP_PRINT_UWORD('u', 0, 1, bitsize);
                }
                STEP_PRINT_STRING(">>");
                cur->sub = PRINT_TERM_CURSOR_NORMAL;
            } else {
                STEP_PRINT_STRING("<<\"");
            L_resume_printbin:
                while (bytesize) {
                    if (res >= max_bytes) {
                        cur->bin_bytep = bytep;
                        cur->bin_bytesize = bytesize;
                        cur->bin_bitoffs = bitoffs;
                        cur->bin_bitsize = bitsize;
                        cur->sub = PRINT_TERM_CURSOR_PRINTBIN;
                        return 1;
                    }
                    if (bitoffs)
                        octet = (bytep[0] << bitoffs) | (bytep[1] >> (8 - bitoffs));
                    else
                        octet = bytep[0];
                    if (octet == '"')
                        STEP_PRINT_CHAR('\\');
                    STEP_PRINT_CHAR(octet);
                    ++bytep;
                    --bytesize;
                }
                STEP_PRINT_STRING("\">>");
                cur->sub = PRINT_TERM_CURSOR_NORMAL;
            }
            break;
        case FUN_DEF:
            funp = (ErlFunThing *) fun_val(wobj);

            if (is_local_fun(funp)) {
                fe = funp->entry.fun;
                ap = atom_tab(atom_val(fe->module));

                STEP_PRINT_STRING("#Fun<");
                STEP_PRINT_BUF(erts_atom_get_name(ap), ap->len);
                STEP_PRINT_CHAR('.');
                STEP_PRINT_SWORD('d', 0, 1, (ErlPfSWord) fe->old_index);
                STEP_PRINT_CHAR('.');
                STEP_PRINT_SWORD('d', 0, 1, (ErlPfSWord) fe->old_uniq);
                STEP_PRINT_CHAR('>');
            } else {
                ep = funp->entry.exp;

                STEP_PRINT_STRING("fun ");

                tdcount = LONG_MAX;
                tres = print_atom_name(fn, arg, ep->info.mfa.module, &tdcount);
                if (tres < 0)
                    STEP_FAIL();
                res += tres;

                STEP_PRINT_CHAR(':');

                tdcount = LONG_MAX;
                tres = print_atom_name(fn, arg, ep->info.mfa.function, &tdcount);
                if (tres < 0)
                    STEP_FAIL();
                res += tres;

                STEP_PRINT_CHAR('/');
                STEP_PRINT_SWORD('d', 0, 1, (ErlPfSWord) ep->info.mfa.arity);
            }
            break;
        case MAP_DEF:
            head = boxed_val(wobj);

            if (is_flatmap_header(*head)) {
                n = flatmap_get_size(head);
                ks = flatmap_get_keys(head);
                vs = flatmap_get_values(head);

                STEP_PRINT_CHAR('#');
                STEP_PRINT_CHAR('{');
                WSTACK_PUSH(ws, PRT_CLOSE_TUPLE);
                if (n > 0) {
                    n--;
                    WSTACK_PUSH5(ws, vs[n], PRT_TERM, PRT_ASSOC, ks[n], PRT_TERM);
                    while (n--) {
                        WSTACK_PUSH6(ws, PRT_COMMA, vs[n], PRT_TERM, PRT_ASSOC,
                                     ks[n], PRT_TERM);
                    }
                }
            } else {
                mapval = MAP_HEADER_VAL(*head);
                switch (MAP_HEADER_TYPE(*head)) {
                case MAP_HEADER_TAG_HAMT_HEAD_ARRAY:
                case MAP_HEADER_TAG_HAMT_HEAD_BITMAP:
                    STEP_PRINT_STRING("#{");
                    WSTACK_PUSH(ws, PRT_CLOSE_TUPLE);
                    head++;
                    ERTS_FALLTHROUGH();
                case MAP_HEADER_TAG_HAMT_NODE_BITMAP:
                    n = hashmap_bitcount(mapval);
                    ASSERT(0 < n && n < 17);
                    while (1) {
                        if (is_list(head[n])) {
                            assoc = list_val(head[n]);
                            key = CAR(assoc);
                            val = CDR(assoc);
                            WSTACK_PUSH5(ws, val, PRT_TERM, PRT_ASSOC, key, PRT_TERM);
                        } else if (is_tuple(head[n])) {
                            tpl = tuple_val(head[n]);
                            i = arityval(tpl[0]);
                            ASSERT(i >= 2);
                            while (1) {
                                assoc = list_val(tpl[i]);
                                key = CAR(assoc);
                                val = CDR(assoc);
                                WSTACK_PUSH5(ws, val, PRT_TERM, PRT_ASSOC, key, PRT_TERM);
                                if (--i == 0)
                                    break;
                                WSTACK_PUSH(ws, PRT_COMMA);
                            }
                        } else {
                            WSTACK_PUSH2(ws, head[n], PRT_TERM);
                        }
                        if (--n == 0)
                            break;
                        WSTACK_PUSH(ws, PRT_COMMA);
                    }
                    break;
                }
            }
            break;
        case BIN_REF_DEF:
            STEP_PRINT_STRING("#BinRef");
            break;
        default:
            STEP_PRINT_STRING("<unknown:");
            STEP_PRINT_POINTER(wobj);
            STEP_PRINT_CHAR('>');
            break;
        }
    }

 L_done:
    erts_print_term_cursor_destroy(cur);
    return 0;
#undef STEP_PRINT_ATOM
#undef STEP_PRINT_DOUBLE
#undef STEP_PRINT_UWORD64
#undef STEP_PRINT_SWORD
#undef STEP_PRINT_UWORD
#undef STEP_PRINT_POINTER
#undef STEP_PRINT_BUF
#undef STEP_PRINT_STRING
#undef STEP_PRINT_CHAR
#undef STEP_FAIL
#undef ws
}
