/*
    $Id: errors_e.h 1229 2016-07-09 19:52:59Z soci $

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
#ifndef ERRORS_E_H
#define ERRORS_E_H

/* ---------------------------------------------------------------------------
   $00-$3f warning
   $40-$7f error
   $80-$bf fatal
*/

enum errors_e {
    ERROR___LONG_BRANCH = 0x00,
    ERROR_DIRECTIVE_IGN,
    ERROR____OLD_MODULO,
    ERROR_______OLD_NEQ,
    ERROR______OLD_GOTO,
    ERROR_____TAIL_CALL,
    ERROR___CONST_INDEX,
    ERROR_WUSER_DEFINED,
    ERROR___OPTIMIZABLE,
    ERROR_____REMOVABLE,
    ERROR_____OLD_EQUAL,

    ERROR__DOUBLE_RANGE = 0x40,
    ERROR_DOUBLE_ESCAPE,
    ERROR_EXTRA_CHAR_OL,
    ERROR_CONSTNT_LARGE,
    ERROR_NUMERIC_OVERF,
    ERROR_ADDRESS_LARGE,
    ERROR_GENERL_SYNTAX,
    ERROR_EXPRES_SYNTAX,
    ERROR_LABEL_REQUIRE,
    ERROR_MISSING_ARGUM,
    ERROR_DIVISION_BY_Z,
    ERROR_NO_ZERO_VALUE,
    ERROR___NO_HIGH_BIT,
    ERROR__BYTES_NEEDED,
    ERROR___NO_LAST_GAP,
    ERROR_CANT_CROSS_BA,
    ERROR_OUTOF_SECTION,
    ERROR_NEGFRAC_POWER,
    ERROR_SQUARE_ROOT_N,
    ERROR_LOG_NON_POSIT,
    ERROR___MATH_DOMAIN,
    ERROR___EMPTY_RANGE,
    ERROR__EMPTY_STRING,
    ERROR_BIG_STRING_CO,
    ERROR____NO_FORWARD,
    ERROR_REQUIREMENTS_,
    ERROR______CONFLICT,
    ERROR___INDEX_RANGE,
    ERROR__NOT_HASHABLE,
    ERROR__NOT_KEYVALUE,
    ERROR_____CANT_IVAL,
    ERROR_____CANT_UVAL,
    ERROR_CANT_BROADCAS,
    ERROR_____CANT_SIGN,
    ERROR______CANT_ABS,
    ERROR______CANT_INT,
    ERROR______CANT_LEN,
    ERROR_____CANT_SIZE,
    ERROR_____CANT_BOOL,
    ERROR______NOT_ITER,
    ERROR__NO_BYTE_ADDR,
    ERROR__NO_WORD_ADDR,
    ERROR__NO_LONG_ADDR,
    ERROR____NOT_DIRECT,
    ERROR__NOT_DATABANK,
    ERROR_____NOT_BANK0,
    ERROR_OUT_OF_MEMORY,
    ERROR__ADDR_COMPLEX,
    ERROR__USER_DEFINED,
    ERROR_NO_ADDRESSING,
    ERROR___NO_REGISTER,
    ERROR___NO_LOT_OPER,
    ERROR____PAGE_ERROR,
    ERROR__BRANCH_CROSS,
    ERROR_BRANCH_TOOFAR,
    ERROR____PTEXT_LONG,
    ERROR___UNKNOWN_CHR,
    ERROR______EXPECTED,
    ERROR___NOT_ALLOWED,
    ERROR_RESERVED_LABL,
    ERROR___UNKNOWN_CPU,

    ERROR___NOT_DEFINED,
    ERROR__INVALID_OPER,
    ERROR____STILL_NONE,
    ERROR_____KEY_ERROR,

    ERROR_CANT_FINDFILE = 0xc0,
    ERROR__READING_FILE,
    ERROR_CANT_WRTE_OBJ,
    ERROR_CANT_WRTE_LST,
    ERROR_CANT_WRTE_LBL,
    ERROR_CANT_WRTE_MAK,
    ERROR_CANT_WRTE_ERR,
    ERROR_FILERECURSION,
    ERROR__MACRECURSION,
    ERROR__FUNRECURSION,
    ERROR_TOO_MANY_PASS,
    ERROR_UNKNOWN_OPTIO
};
#endif
