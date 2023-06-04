#include "bridge.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unicorn/unicorn.h>
#include "mysprintf.h"
#include "virtualmemory.h"
#include "mysprintf.h"

#include <float.h>


typedef char* my_va_list;

// internal flag definitions
#define FLAGS_ZEROPAD   (1U <<  0U)
#define FLAGS_LEFT      (1U <<  1U)
#define FLAGS_PLUS      (1U <<  2U)
#define FLAGS_SPACE     (1U <<  3U)
#define FLAGS_HASH      (1U <<  4U)
#define FLAGS_UPPERCASE (1U <<  5U)
#define FLAGS_CHAR      (1U <<  6U)
#define FLAGS_SHORT     (1U <<  7U)
#define FLAGS_LONG      (1U <<  8U)
#define FLAGS_LONG_LONG (1U <<  9U)
#define FLAGS_PRECISION (1U << 10U)
#define FLAGS_ADAPT_EXP (1U << 11U)


#define PRINTF_NTOA_BUFFER_SIZE    32U
#define PRINTF_FTOA_BUFFER_SIZE    32U
#define PRINTF_MAX_FLOAT  1e9
#define PRINTF_DEFAULT_FLOAT_PRECISION  6U
#define PRINTF_SUPPORT_FLOAT
#define PRINTF_SUPPORT_LONG_LONG
#define PRINTF_SUPPORT_PTRDIFF_T



// internal secure strlen
// \return The length of the string (excluding the terminating 0) limited by 'maxsize'
static inline unsigned int _strnlen_s(const char* str, uint32_t maxsize)
{
    const char* s;
    for (s = str; *s && maxsize--; ++s);
    return (unsigned int)(s - str);
}


// internal test if char is a digit (0-9)
// \return true if char is a digit
static inline bool _is_digit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}


// internal ASCII string to unsigned int conversion
static unsigned int _atoi(const char** str)
{
    unsigned int i = 0U;
    while (_is_digit(**str)) {
        i = i * 10U + (unsigned int)(*((*str)++) - '0');
    }
    return i;
}



// internal buffer output
static void _out_buffer(char character, void* buffer, uint32_t idx, uint32_t maxlen)
{
    if (idx < maxlen) {
        ((char*)buffer)[idx] = character;
    }
}

// output function type
typedef void (*out_fct_type)(char character, void* buffer, uint32_t idx, uint32_t maxlen);


// output the specified string in reverse, taking care of any zero-padding
static int32_t _out_rev(out_fct_type out, char* buffer, int32_t idx, int32_t maxlen, const char* buf, int32_t len, unsigned int width, unsigned int flags)
{
    const int32_t start_idx = idx;
    int32_t i;

    // pad spaces up to given width
    if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
        for (i = len; i < width; i++) {
            out(' ', buffer, idx++, maxlen);
        }
    }

    // reverse string
    while (len) {
        out(buf[--len], buffer, idx++, maxlen);
    }

    // append pad spaces up to given width
    if (flags & FLAGS_LEFT) {
        while (idx - start_idx < width) {
            out(' ', buffer, idx++, maxlen);
        }
    }

    return idx;
}

// internal itoa format
static int32_t _ntoa_format(out_fct_type out, char* buffer, int32_t idx, int32_t maxlen, char* buf, int32_t len, bool negative, unsigned int base, unsigned int prec, unsigned int width, unsigned int flags)
{
    // pad leading zeros
    if (!(flags & FLAGS_LEFT)) {
        if (width && (flags & FLAGS_ZEROPAD) && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
            width--;
        }
        while ((len < prec) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
            buf[len++] = '0';
        }
        while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
            buf[len++] = '0';
        }
    }

    // handle hash
    if (flags & FLAGS_HASH) {
        if (!(flags & FLAGS_PRECISION) && len && ((len == prec) || (len == width))) {
            len--;
            if (len && (base == 16U)) {
                len--;
            }
        }
        if ((base == 16U) && !(flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
            buf[len++] = 'x';
        }
        else if ((base == 16U) && (flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
            buf[len++] = 'X';
        }
        else if ((base == 2U) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
            buf[len++] = 'b';
        }
        if (len < PRINTF_NTOA_BUFFER_SIZE) {
            buf[len++] = '0';
        }
    }

    if (len < PRINTF_NTOA_BUFFER_SIZE) {
        if (negative) {
            buf[len++] = '-';
        }
        else if (flags & FLAGS_PLUS) {
            buf[len++] = '+';  // ignore the space if the '+' exists
        }
        else if (flags & FLAGS_SPACE) {
            buf[len++] = ' ';
        }
    }

    return _out_rev(out, buffer, idx, maxlen, buf, len, width, flags);
}


// internal itoa for 'long' type
static int32_t _ntoa_long(out_fct_type out, char* buffer, int32_t idx, int32_t maxlen, unsigned long value, bool negative, unsigned long base, unsigned int prec, unsigned int width, unsigned int flags)
{
    char buf[PRINTF_NTOA_BUFFER_SIZE];
    int32_t len = 0U;

    // no hash for 0 values
    if (!value) {
        flags &= ~FLAGS_HASH;
    }

    // write if precision != 0 and value is != 0
    if (!(flags & FLAGS_PRECISION) || value) {
        do {
            const char digit = (char)(value % base);
            buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
            value /= base;
        } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
    }

    return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative, (unsigned int)base, prec, width, flags);
}

static int32_t _ntoa_long_long(out_fct_type out, char* buffer, int32_t idx, int32_t maxlen, unsigned long long value, bool negative, unsigned long long base, unsigned int prec, unsigned int width, unsigned int flags)
{
    char buf[PRINTF_NTOA_BUFFER_SIZE];
    int32_t len = 0U;

    // no hash for 0 values
    if (!value) {
        flags &= ~FLAGS_HASH;
    }

    // write if precision != 0 and value is != 0
    if (!(flags & FLAGS_PRECISION) || value) {
        do {
            const char digit = (char)(value % base);
            buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
            value /= base;
        } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
    }

    return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative, (unsigned int)base, prec, width, flags);
}


// internal ftoa for fixed decimal floating point
static int32_t _ftoa(out_fct_type out, char* buffer, int32_t idx, int32_t maxlen, double value, unsigned int prec, unsigned int width, unsigned int flags)
{
    char buf[PRINTF_FTOA_BUFFER_SIZE];
    int32_t len = 0U;
    double diff = 0.0;
    bool negative = false;
    int whole;
    double tmp;
    unsigned long frac;

    // powers of 10
    static const double pow10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };

    // test for special values
    if (value != value)
        return _out_rev(out, buffer, idx, maxlen, "nan", 3, width, flags);
    if (value < -DBL_MAX)
        return _out_rev(out, buffer, idx, maxlen, "fni-", 4, width, flags);
    if (value > DBL_MAX)
        return _out_rev(out, buffer, idx, maxlen, (flags & FLAGS_PLUS) ? "fni+" : "fni", (flags & FLAGS_PLUS) ? 4U : 3U, width, flags);

    // test for very large values
    // standard printf behavior is to print EVERY whole number digit -- which could be 100s of characters overflowing your buffers == bad
    if ((value > PRINTF_MAX_FLOAT) || (value < -PRINTF_MAX_FLOAT)) {
#if defined(PRINTF_SUPPORT_EXPONENTIAL)
        return _etoa(out, buffer, idx, maxlen, value, prec, width, flags);
#else
        return 0U;
#endif
    }

    // test for negative
    if (value < 0) {
        negative = true;
        value = 0 - value;
    }

    // set default precision, if not set explicitly
    if (!(flags & FLAGS_PRECISION)) {
        prec = PRINTF_DEFAULT_FLOAT_PRECISION;
    }
    // limit precision to 9, cause a prec >= 10 can lead to overflow errors
    while ((len < PRINTF_FTOA_BUFFER_SIZE) && (prec > 9U)) {
        buf[len++] = '0';
        prec--;
    }

    whole = (int)value;
    tmp = (value - whole) * pow10[prec];
    frac = (unsigned long)tmp;
    diff = tmp - frac;

    if (diff > 0.5) {
        ++frac;
        // handle rollover, e.g. case 0.99 with prec 1 is 1.0
        if (frac >= pow10[prec]) {
            frac = 0;
            ++whole;
        }
    }
    else if (diff < 0.5) {
    }
    else if ((frac == 0U) || (frac & 1U)) {
        // if halfway, round up if odd OR if last digit is 0
        ++frac;
    }

    if (prec == 0U) {
        diff = value - (double)whole;
        if ((!(diff < 0.5) || (diff > 0.5)) && (whole & 1)) {
            // exactly 0.5 and ODD, then round up
            // 1.5 -> 2, but 2.5 -> 2
            ++whole;
        }
    }
    else {
        unsigned int count = prec;
        // now do fractional part, as an unsigned number
        while (len < PRINTF_FTOA_BUFFER_SIZE) {
            --count;
            buf[len++] = (char)(48U + (frac % 10U));
            if (!(frac /= 10U)) {
                break;
            }
        }
        // add extra 0s
        while ((len < PRINTF_FTOA_BUFFER_SIZE) && (count-- > 0U)) {
            buf[len++] = '0';
        }
        if (len < PRINTF_FTOA_BUFFER_SIZE) {
            // add decimal
            buf[len++] = '.';
        }
    }

    // do whole part, number is reversed
    while (len < PRINTF_FTOA_BUFFER_SIZE) {
        buf[len++] = (char)(48 + (whole % 10));
        if (!(whole /= 10)) {
            break;
        }
    }

    // pad leading zeros
    if (!(flags & FLAGS_LEFT) && (flags & FLAGS_ZEROPAD)) {
        if (width && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
            width--;
        }
        while ((len < width) && (len < PRINTF_FTOA_BUFFER_SIZE)) {
            buf[len++] = '0';
        }
    }

    if (len < PRINTF_FTOA_BUFFER_SIZE) {
        if (negative) {
            buf[len++] = '-';
        }
        else if (flags & FLAGS_PLUS) {
            buf[len++] = '+';  // ignore the space if the '+' exists
        }
        else if (flags & FLAGS_SPACE) {
            buf[len++] = ' ';
        }
    }

    return _out_rev(out, buffer, idx, maxlen, buf, len, width, flags);
}

// internal vsnprintf
static int my_vsnprintf(uc_engine* uc, out_fct_type out, char* buffer, const uint32_t maxlen, const char* format, my_va_list va)
{
    unsigned int flags, width, precision, n;
    int32_t idx = 0U;

    if (!buffer) {
        // use null output function
        out = NULL;
    }

    while (*format)
    {
        // format specifier?  %[flags][width][.precision][length]
        if (*format != '%') {
            // no
            out(*format, buffer, idx++, maxlen);
            format++;
            continue;
        }
        else {
            // yes, evaluate it
            format++;
        }

        // evaluate flags
        flags = 0U;
        do {
            switch (*format) {
            case '0': flags |= FLAGS_ZEROPAD; format++; n = 1U; break;
            case '-': flags |= FLAGS_LEFT;    format++; n = 1U; break;
            case '+': flags |= FLAGS_PLUS;    format++; n = 1U; break;
            case ' ': flags |= FLAGS_SPACE;   format++; n = 1U; break;
            case '#': flags |= FLAGS_HASH;    format++; n = 1U; break;
            default:                                   n = 0U; break;
            }
        } while (n);

        // evaluate width field
        width = 0U;
        if (_is_digit(*format)) {
            width = _atoi(&format);
        }
        else if (*format == '*') {
            int w = *((int *)va); va += sizeof(int);
            if (w < 0) {
                flags |= FLAGS_LEFT;    // reverse padding
                width = (unsigned int)-w;
            }
            else {
                width = (unsigned int)w;
            }
            format++;
        }

        // evaluate precision field
        precision = 0U;
        if (*format == '.') {
            flags |= FLAGS_PRECISION;
            format++;
            if (_is_digit(*format)) {
                precision = _atoi(&format);
            }
            else if (*format == '*') {
                int prec = *((int*)va); va += sizeof(int);
                precision = prec > 0 ? (unsigned int)prec : 0U;
                format++;
            }
        }

        // evaluate length field
        switch (*format) {
        case 'l':
            flags |= FLAGS_LONG;
            format++;
            if (*format == 'l') {
                flags |= FLAGS_LONG_LONG;
                format++;
            }
            break;
        case 'h':
            flags |= FLAGS_SHORT;
            format++;
            if (*format == 'h') {
                flags |= FLAGS_CHAR;
                format++;
            }
            break;
#if defined(PRINTF_SUPPORT_PTRDIFF_T)
        case 't':
            flags |= (sizeof(ptrdiff_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
            format++;
            break;
#endif
        case 'j':
            flags |= (sizeof(intmax_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
            format++;
            break;
        case 'z':
            flags |= (sizeof(int32_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
            format++;
            break;
        default:
            break;
        }

        // evaluate specifier
        switch (*format) {
        case 'd':
        case 'i':
        case 'u':
        case 'x':
        case 'X':
        case 'o':
        case 'b': {
            // set the base
            unsigned int base;
            if (*format == 'x' || *format == 'X') {
                base = 16U;
            }
            else if (*format == 'o') {
                base = 8U;
            }
            else if (*format == 'b') {
                base = 2U;
            }
            else {
                base = 10U;
                flags &= ~FLAGS_HASH;   // no hash for dec format
            }
            // uppercase
            if (*format == 'X') {
                flags |= FLAGS_UPPERCASE;
            }

            // no plus or space flag for u, x, X, o, b
            if ((*format != 'i') && (*format != 'd')) {
                flags &= ~(FLAGS_PLUS | FLAGS_SPACE);
            }

            // ignore '0' flag when precision is given
            if (flags & FLAGS_PRECISION) {
                flags &= ~FLAGS_ZEROPAD;
            }

            // convert the integer
            if ((*format == 'i') || (*format == 'd')) {
                // signed
                if (flags & FLAGS_LONG_LONG) {
#if defined(PRINTF_SUPPORT_LONG_LONG)
                    long long value = *((long long*)va); va += sizeof(long long);
                    idx = _ntoa_long_long(out, buffer, idx, maxlen, (unsigned long long)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
#endif
                }
                else if (flags & FLAGS_LONG) {
                    long value = *((long*)va);va += sizeof(long);
                    idx = _ntoa_long(out, buffer, idx, maxlen, (unsigned long)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
                }
                else {
                    int value = 0;
                    if (flags & FLAGS_CHAR)
                    {
                         value = *((int*)va); va += sizeof(int);
                    }
                    else
                    {
                        if (flags & FLAGS_SHORT)
                        {
                            value = (short int)(*((int*)va)); va += sizeof(int);
                        }
                        else
                        {
                            value = *((int*)va); va += sizeof(int);
                        }
                    }

                    idx = _ntoa_long(out, buffer, idx, maxlen, (unsigned int)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
                }
            }
            else {
                // unsigned
                if (flags & FLAGS_LONG_LONG) {
#if defined(PRINTF_SUPPORT_LONG_LONG)
                    unsigned long long value = *((unsigned long long*)va); va += sizeof(unsigned long long);
                    idx = _ntoa_long_long(out, buffer, idx, maxlen, value, false, base, precision, width, flags);
#endif
                }
                else if (flags & FLAGS_LONG) {
                    unsigned long value = *((unsigned long*)va); va += sizeof(unsigned long);
                    idx = _ntoa_long(out, buffer, idx, maxlen, value, false, base, precision, width, flags);
                }
                else {
                    unsigned int value = 0;
                    
                    if (flags & FLAGS_CHAR)
                    {
                        unsigned int v = *((unsigned int*)va); va += sizeof(unsigned int);
                        value = (unsigned char)v;
                    }
                    else
                    {
                        if (flags & FLAGS_SHORT)
                        {
                            unsigned int v = *((unsigned int*)va); va += sizeof(unsigned int);
                            value = (unsigned short int)v;
                        }
                        else
                        {
                            value = *((unsigned int*)va); va += sizeof(unsigned int);
                        }
                    } 
                    idx = _ntoa_long(out, buffer, idx, maxlen, value, false, base, precision, width, flags);
                }
            }
            format++;
            break;
        }
#if defined(PRINTF_SUPPORT_FLOAT)
        case 'f':
        case 'F':
        {
            if (*format == 'F') flags |= FLAGS_UPPERCASE;
            double value = *((double*)va); va += sizeof(double);
            idx = _ftoa(out, buffer, idx, maxlen, value, precision, width, flags);
            format++;
            break;
        }
#if defined(PRINTF_SUPPORT_EXPONENTIAL)
        case 'e':
        case 'E':
        case 'g':
        case 'G':
            if ((*format == 'g') || (*format == 'G')) flags |= FLAGS_ADAPT_EXP;
            if ((*format == 'E') || (*format == 'G')) flags |= FLAGS_UPPERCASE;
            idx = _etoa(out, buffer, idx, maxlen, my_va_arg(va, double), precision, width, flags);
            format++;
            break;
#endif  // PRINTF_SUPPORT_EXPONENTIAL
#endif  // PRINTF_SUPPORT_FLOAT
        case 'c': {
            unsigned int l = 1U;
            int value = 0;
            // pre padding
            if (!(flags & FLAGS_LEFT)) {
                while (l++ < width) {
                    out(' ', buffer, idx++, maxlen);
                }
            }
            // char output
            value = *((int*)va); va += sizeof(int);
            out((char)value, buffer, idx++, maxlen);
            // post padding
            if (flags & FLAGS_LEFT) {
                while (l++ < width) {
                    out(' ', buffer, idx++, maxlen);
                }
            }
            format++;
            break;
        }

        case 's': {
            uint32_t value = 0;
            value = *((uint32_t*)va); va += sizeof(uint32_t);
            char* p = (char*)toHostPtr(value);
            unsigned int l = _strnlen_s(p, precision ? precision : (int32_t)-1);
            // pre padding
            if (flags & FLAGS_PRECISION) {
                l = (l < precision ? l : precision);
            }
            if (!(flags & FLAGS_LEFT)) {
                while (l++ < width) {
                    out(' ', buffer, idx++, maxlen);
                }
            }
            // string output
            while ((*p != 0) && (!(flags & FLAGS_PRECISION) || precision--)) {
                out(*(p++), buffer, idx++, maxlen);
            }
            // post padding
            if (flags & FLAGS_LEFT) {
                while (l++ < width) {
                    out(' ', buffer, idx++, maxlen);
                }
            }
            format++;
            break;
        }

        case 'p': {
            bool is_ll;
            width = sizeof(void*) * 2U;
            flags |= FLAGS_ZEROPAD | FLAGS_UPPERCASE;
#if defined(PRINTF_SUPPORT_LONG_LONG)
            is_ll = sizeof(long long);
            if (is_ll) {
                uint32_t value = *((uint32_t*)va); va += sizeof(uint32_t);
                idx = _ntoa_long_long(out, buffer, idx, maxlen, value, false, 16U, precision, width, flags);
            }
            else {
#endif
                uint32_t value = *((uint32_t*)va); va += sizeof(uint32_t);
                idx = _ntoa_long(out, buffer, idx, maxlen, (unsigned long)value, false, 16U, precision, width, flags);
#if defined(PRINTF_SUPPORT_LONG_LONG)
            }
#endif
            format++;
            break;
        }

        case '%':
            out('%', buffer, idx++, maxlen);
            format++;
            break;

        default:
            out(*format, buffer, idx++, maxlen);
            format++;
            break;
        }
    }

    // termination
    out((char)0, buffer, idx < maxlen ? idx : maxlen - 1U, maxlen);

    // return written chars without terminating \0
    return (int)idx;
}

void my_sprintf(uc_engine* uc)
{
    uint32_t a0, a1, a2, a3;
    uint32_t sp;

    uc_reg_read(uc, UC_MIPS_REG_A0, &a0);
    uc_reg_read(uc, UC_MIPS_REG_A1, &a1);
    uc_reg_read(uc, UC_MIPS_REG_A2, &a2);
    uc_reg_read(uc, UC_MIPS_REG_A3, &a3);

    uc_reg_read(uc, UC_MIPS_REG_SP, &sp);

    char* buffer = (char*)toHostPtr((a0));
    char* format = (char*)toHostPtr((a1));

    uint32_t s1, s2;

    sp -= 40;

    s1 = ((uint32_t*)toHostPtr((sp + 48)))[0];
    s2 = ((uint32_t*)toHostPtr((sp + 52)))[0];

    *((uint32_t*)toHostPtr((sp + 48))) = a2;
    *((uint32_t*)toHostPtr((sp + 52))) = a3;
    my_va_list va = (my_va_list)toHostPtr((sp + 48));

    uint32_t ret;
    ret = my_vsnprintf(uc, _out_buffer, buffer, (uint32_t)-1, format, va);

    *((uint32_t*)toHostPtr((sp + 48))) = s1;
    *((uint32_t*)toHostPtr((sp + 52))) = s2;

    //printf("my_vsnprintf: buffer %s, format %s\n", buffer, format);

    uc_reg_write(uc, UC_MIPS_REG_V0, &ret);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}

void dingoo_debug(uc_engine* uc)
{
    uint32_t a0, a1, a2, a3;
    uint32_t sp;

    uc_reg_read(uc, UC_MIPS_REG_A0, &a0);
    uc_reg_read(uc, UC_MIPS_REG_A1, &a1);
    uc_reg_read(uc, UC_MIPS_REG_A2, &a2);
    uc_reg_read(uc, UC_MIPS_REG_A3, &a3);

    uc_reg_read(uc, UC_MIPS_REG_SP, &sp);

    char buffer[128];
    char* format = (char*)toHostPtr((a0));

    *(uint32_t*)toHostPtr((sp + 4))     = a1;
    *(uint32_t*)toHostPtr((sp + 8))     = a2;
    *(uint32_t*)toHostPtr((sp + 0xc))   = a3;

    my_va_list va = (my_va_list)toHostPtr((sp + 4));

    uint32_t ret;
    ret = my_vsnprintf(uc, _out_buffer, buffer, (uint32_t)-1, format, va);

    printf("s3d_dbg: %s", buffer);

    uint32_t pc;
    uc_reg_read(uc, UC_MIPS_REG_RA, &pc);
    uc_reg_write(uc, UC_MIPS_REG_PC, &pc);
}
