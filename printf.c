// printf without any header or library only works on Linux aarch64

/*
 * Conditional compilation: Ensures this code only runs on Linux aarch64 LP64
 * __linux__ : Defined when compiling for Linux
 * __aarch64__ : Defined when compiling for ARM64 architecture
 * __LP64__ : Defined when using LP64 data model (long and pointer are 64-bit)
 * NOTE: This also works on Android since Android uses Linux kernel
 */
#if defined(__linux__) && defined(__aarch64__) && defined(__LP64__)
/* Linux aarch64 */
#else
#error "This code is only work in linux aarch64 LP64 (including Android)"
#endif

// Define basic data types
typedef unsigned long int size_t;
typedef long ssize_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef long int64_t;
typedef double double_t;

// Define va_list
typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(last)       __builtin_va_end(last)

// File descriptor for standard output is 1 thats way 1> or 2> for stderr
// see: https://en.wikipedia.org/wiki/Standard_streams
// for more information
// NOTE: stdin is 0, stdout is 1, stderr is 2
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// System call number for write() on ARM64 Linux
// You can find this in: /usr/include/asm-generic/unistd.h
// or check with: grep "__NR_write" /usr/include/asm-generic/unistd.h
#define __NR_write 64
#define __NR_read  63
// syscall for exit can also found in asm-generic/unistd.h
#define __NR_exit 93

// Exit status codes
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define NULL ((void *)0)

// Math constants for floating point
#define DBL_MAX  1.7976931348623157e+308
#define DBL_MIN  2.2250738585072014e-308
#define INFINITY (1.0 / 0.0)
#define NAN      (0.0 / 0.0)

/*
 * System call convention for ARM64:
 * - x0: first argument (file descriptor)
 * - x1: second argument (buffer pointer)
 * - x2: third argument (count)
 * - x8: system call number
 * - svc 0: supervisor call (like int 0x80 on x86)
 * Return value is in x0
 */
static inline ssize_t
syscall(int fd, long syscall_number, ssize_t buf, size_t count) {
    // Load arguments into registers following ARM64 calling convention
    register long x0 asm("x0") = fd;             // File descriptor
    register long x1 asm("x1") = buf;            // Buffer address
    register long x2 asm("x2") = count;          // Number of bytes to write
    register long x8 asm("x8") = syscall_number; // System call number

    // Inline assembly for system call
    asm volatile(
        "svc 0"    // Make supervisor call (system call)
        : "+r"(x0) // Output: x0 (return value) is both input and output
        : "r"(x1), "r"(x2), "r"(x8) // Input: x1, x2, x8 registers
        : "memory"                  // Clobber: memory may be modified
    );

    // About svc see:
    // https://s-o-c.org/arm-svc-instruction-example/#what-is-the-svc-instruction

    return x0; // Return value from system call (negative for error)
}

static ssize_t write(int fd, const void *buf, size_t count) {
    return syscall(fd, __NR_write, (long)buf, count);
}

static inline ssize_t read(int fd, void *buf, size_t count) {
    return syscall(fd, __NR_read, (long)buf, count);
}

// About __attribute__((noreturn)) see:
// https://stackoverflow.com/questions/70683911/why-when-would-should-you-use-attribute-noreturn
// About __attribute__ see:
// https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Function-Attributes.html
__attribute__((noreturn)) static inline void exit(int exit_code) {
    register long x0 asm("x0") = exit_code;
    register long x8 asm("x8") = __NR_exit;

    asm volatile( //
        "svc 0"   // Make supervisor call (system call)
        :         // empty, because not return anyting
        : "r"(x0), "r"(x8)
        : "memory"

    );

    // trigger trap instruction if syscall fail
    __builtin_trap();
    // see:
    // https://developer.arm.com/documentation/107976/20-1-0/Clang-reference/clang-built-in-functions/--builtin-trap
}

/*
 * Custom strlen() implementation
 * Since we're not using standard library, we need to implement this ourselves
 * Returns the length of a null-terminated string
 */
size_t strlen(const char *s) {
    const char *format = s; // Start at beginning of string

    // Loop until we find null terminator
    while (*format != '\0') {
        format++;
    }

    // Calculate length: end pointer minus start pointer
    return (size_t)(format - s);
}

/*
 * Custom memcpy() implementation
 * Copies n bytes from src to dest
 */
void *memcpy(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dest;
}

/*
 * Custom memset() implementation
 * Sets n bytes to value c
 */
void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

// Helper functions for floating point
static inline int isinf(double x) {
    uint64_t bits;
    memcpy(&bits, &x, sizeof(double));
    // Check if exponent is all 1s and mantissa is 0
    return ((bits & 0x7FF0000000000000) == 0x7FF0000000000000) &&
           ((bits & 0x000FFFFFFFFFFFFF) == 0);
}

static inline int isnan(double x) {
    uint64_t bits;
    memcpy(&bits, &x, sizeof(double));
    // Check if exponent is all 1s and mantissa is non-zero
    return ((bits & 0x7FF0000000000000) == 0x7FF0000000000000) &&
           ((bits & 0x000FFFFFFFFFFFFF) != 0);
}

/*
 * Number conversion functions
 */

// Convert unsigned integer to string with given base (2-16)
// Return pointer to null terminator, not to buffer start
static char *uitoa(uint64_t num, char *buffer, int base, int uppercase) {
    char digits_lower[] = "0123456789abcdef";
    char digits_upper[] = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_upper : digits_lower;

    char *ptr = buffer;
    char *start = buffer;

    // Handle 0 explicitly
    if (num == 0) {
        *ptr++ = '0';
    } else {
        // Convert number to string (reverse order)
        while (num > 0) {
            *ptr++ = digits[num % base];
            num /= base;
        }

        // Reverse the string
        char *end = ptr - 1;
        while (start < end) {
            char tmp = *start;
            *start = *end;
            *end = tmp;
            start++;
            end--;
        }
    }

    *ptr = '\0';
    return ptr; // Return end pointer
}

// Convert double to string with fixed notation
// Returns pointer to null terminator
static char *ftoa(double value,
                  char *buffer,
                  int precision,
                  int uppercase,
                  int alternate_form) {

    char *ptr = buffer;

    // Handle special cases
    if (isnan(value)) {
        const char *nan_str = uppercase ? "NAN" : "nan";
        while (*nan_str) {
            *ptr++ = *nan_str++;
        }
        *ptr = '\0';
        return ptr;
    }

    if (isinf(value)) {
        const char *inf_str = uppercase ? "INF" : "inf";
        if (value < 0) {
            *ptr++ = '-';
        }
        while (*inf_str) {
            *ptr++ = *inf_str++;
        }
        *ptr = '\0';
        return ptr;
    }

    // Handle sign
    if (value < 0) {
        *ptr++ = '-';
        value = -value;
    }

    // Default precision is 6 if not specified
    if (precision < 0) {
        precision = 6;
    }

    // Round the number based on precision
    double rounder = 0.5;
    for (int i = 0; i < precision; i++) {
        rounder *= 0.1;
    }
    value += rounder;

    // Extract integer part
    uint64_t int_part = (uint64_t)value;
    double fractional = value - int_part;

    // Convert integer part
    char *int_end = uitoa(int_part, ptr, 10, 0);
    ptr = int_end;

    // Add decimal point if precision > 0 or alternate form is set
    if (precision > 0 || alternate_form) {
        *ptr++ = '.';

        // Convert fractional part
        for (int i = 0; i < precision; i++) {
            fractional *= 10.0;
            int digit = (int)fractional;
            *ptr++ = '0' + digit;
            fractional -= digit;
        }
    }

    *ptr = '\0';
    return ptr;
}

// Convert double to string with scientific notation
static char *etoa(double value, char *buffer, int precision, int uppercase) {
    char *ptr = buffer;

    // Handle special cases
    if (isnan(value)) {
        const char *nan_str = uppercase ? "NAN" : "nan";
        while (*nan_str) {
            *ptr++ = *nan_str++;
        }
        *ptr = '\0';
        return ptr;
    }

    if (isinf(value)) {
        const char *inf_str = uppercase ? "INF" : "inf";
        if (value < 0) {
            *ptr++ = '-';
        }
        while (*inf_str) {
            *ptr++ = *inf_str++;
        }
        *ptr = '\0';
        return ptr;
    }

    // Handle sign
    if (value < 0) {
        *ptr++ = '-';
        value = -value;
    }

    // Default precision is 6 if not specified
    if (precision < 0) {
        precision = 6;
    }

    // Handle zero
    if (value == 0.0) {
        *ptr++ = '0';
        if (precision > 0) {
            *ptr++ = '.';
            for (int i = 0; i < precision; i++) {
                *ptr++ = '0';
            }
        }
        *ptr++ = uppercase ? 'E' : 'e';
        *ptr++ = '+';
        *ptr++ = '0';
        *ptr++ = '0';
        *ptr = '\0';
        return ptr;
    }

    // Calculate exponent
    int exponent = 0;
    while (value >= 10.0) {
        value /= 10.0;
        exponent++;
    }
    while (value < 1.0 && value > 0.0) {
        value *= 10.0;
        exponent--;
    }

    // Round the number based on precision
    double rounder = 0.5;
    for (int i = 0; i < precision; i++) {
        rounder *= 0.1;
    }
    value += rounder;

    // Check if rounding caused value to reach 10.0
    if (value >= 10.0) {
        value /= 10.0;
        exponent++;
    }

    // Convert mantissa
    uint64_t int_part = (uint64_t)value;
    double fractional = value - int_part;

    char *int_end = uitoa(int_part, ptr, 10, 0);
    ptr = int_end;

    // Add decimal point if precision > 0
    if (precision > 0) {
        *ptr++ = '.';

        // Convert fractional part
        for (int i = 0; i < precision; i++) {
            fractional *= 10.0;
            int digit = (int)fractional;
            *ptr++ = '0' + digit;
            fractional -= digit;
        }
    }

    // Add exponent
    *ptr++ = uppercase ? 'E' : 'e';

    if (exponent >= 0) {
        *ptr++ = '+';
    } else {
        *ptr++ = '-';
        exponent = -exponent;
    }

    // Convert exponent (always 2 digits minimum)
    if (exponent < 10) {
        *ptr++ = '0';
        *ptr++ = '0' + exponent;
    } else {
        char exp_buf[4];
        char *exp_end = uitoa(exponent, exp_buf, 10, 0);
        char *exp_ptr = exp_buf;
        while (exp_ptr < exp_end) {
            *ptr++ = *exp_ptr++;
        }
    }

    *ptr = '\0';
    return ptr;
}

// Convert double to string with g/G format
static char *gtoa(double value, char *buffer, int precision, int uppercase) {

    // Handle special cases
    if (isnan(value) || isinf(value)) {
        return etoa(value, buffer, precision, uppercase);
    }

    // Default precision is 6 if not specified
    if (precision < 0) {
        precision = 6;
    }

    // Calculate exponent for scientific notation
    int exponent = 0;
    double abs_value = value < 0 ? -value : value;

    if (abs_value != 0.0) {
        while (abs_value >= 10.0) {
            abs_value /= 10.0;
            exponent++;
        }
        while (abs_value < 1.0) {
            abs_value *= 10.0;
            exponent--;
        }
    }

    // Choose between f and e format
    if (exponent < -4 || exponent >= precision) {
        // Use scientific notation
        // Adjust precision: total digits = precision
        if (precision > 0) {
            precision--;
        }
        return etoa(value, buffer, precision, uppercase);
    } else {
        // Use fixed notation
        // Adjust precision: decimal digits = precision - 1 - exponent
        int decimal_digits = precision - 1 - exponent;
        if (decimal_digits < 0) {
            decimal_digits = 0;
        }
        return ftoa(value, buffer, decimal_digits, uppercase, 0);
    }
}

double __trunctfdf2(long double x) {
    // On ARM64 long double is typically same as double
    // Just reinterpret the bits
    // see:
    // https://developer.android.com/ndk/guides/abis?utm_source=chatgpt.com&hl=id/
    union {
        long double ld;
        double d;
    } u;

    u.ld = x;
    return x;
}

// Flags for format specifiers
typedef struct {
    int left_justify;    // '-'
    int always_sign;     // '+'
    int space_sign;      // ' '
    int zero_pad;        // '0'
    int alternate_form;  // '#'
    int width;           // field width
    int precision;       // precision (-1 means unspecified)
    int length_modifier; // h, hh, l, ll, z, t, j, L
    char specifier;      // d, i, u, o, x, X, f, F, e, E, g, G, c, s, p, n, %
} format_flags;

/*
 * Parse format specifier
 * Returns the number of characters consumed
 */
static int parse_format(const char *format, format_flags *flags) {
    const char *start = format;

    // Initialize flags
    flags->left_justify = 0;
    flags->always_sign = 0;
    flags->space_sign = 0;
    flags->zero_pad = 0;
    flags->alternate_form = 0;
    flags->width = 0;
    flags->precision = -1;
    flags->length_modifier = 0;
    flags->specifier = 0;

    // Parse flags
    while (1) {
        switch (*format) {
        case '-':
            flags->left_justify = 1;
            break;
        case '+':
            flags->always_sign = 1;
            break;
        case ' ':
            flags->space_sign = 1;
            break;
        case '0':
            flags->zero_pad = 1;
            break;
        case '#':
            flags->alternate_form = 1;
            break;
        default:
            goto parse_width;
        }
        format++;
    }

parse_width:
    // Parse width
    if (*format >= '0' && *format <= '9') {
        flags->width = 0;
        while (*format >= '0' && *format <= '9') {
            flags->width = flags->width * 10 + (*format - '0');
            format++;
        }
    } else if (*format == '*') {
        // Width from argument (handled later)
        format++;
        flags->width = -1; // Special marker
    }

    // Parse precision
    if (*format == '.') {
        format++;
        flags->precision = 0;

        if (*format >= '0' && *format <= '9') {
            flags->precision = 0;
            while (*format >= '0' && *format <= '9') {
                flags->precision = flags->precision * 10 + (*format - '0');
                format++;
            }
        } else if (*format == '*') {
            // Precision from argument (handled later)
            format++;
            flags->precision = -2; // Special marker
        }
    }

    // Parse length modifier
    switch (*format) {
    case 'h':
        if (*(format + 1) == 'h') {
            flags->length_modifier = 'H'; // hh
            format += 2;
        } else {
            flags->length_modifier = 'h'; // h
            format++;
        }
        break;
    case 'l':
        if (*(format + 1) == 'l') {
            flags->length_modifier = 'L'; // ll
            format += 2;
        } else {
            flags->length_modifier = 'l'; // l
            format++;
        }
        break;
    case 'j':
        flags->length_modifier = 'j';
        format++;
        break;
    case 'z':
        flags->length_modifier = 'z';
        format++;
        break;
    case 't':
        flags->length_modifier = 't';
        format++;
        break;
    case 'L':
        flags->length_modifier =
            'B'; // Capital L for long double (we'll use double)
        format++;
        break;
    }

    // Parse specifier
    flags->specifier = *format;
    format++;

    return (int)(format - start);
}

/*
 * Write formatted output to buffer
 */
static int format_to_buffer(char *buffer, const char *format, va_list ap) {
    char *ptr = buffer;
    char temp_buffer[256]; // Larger buffer for floating point

    while (*format) {
        if (*format != '%') {
            *ptr++ = *format++;
            continue;
        }

        // Handle %%
        if (*(format + 1) == '%') {
            *ptr++ = '%';
            format += 2;
            continue;
        }

        // Parse format specifier
        format_flags flags;
        int consumed = parse_format(format + 1, &flags);
        format += consumed + 1;

        // Handle width from argument
        if (flags.width == -1) {
            flags.width = va_arg(ap, int);
            if (flags.width < 0) {
                flags.left_justify = 1;
                flags.width = -flags.width;
            }
        }

        // Handle precision from argument
        if (flags.precision == -2) {
            flags.precision = va_arg(ap, int);
            if (flags.precision < 0) {
                flags.precision = -1; // Unspecified
            }
        }

        // Handle different specifiers
        switch (flags.specifier) {
        case 'c': {
            // Character
            char c = (char)va_arg(ap, int);
            *ptr++ = c;
            break;
        }

        case 's': {
            // String
            const char *str = va_arg(ap, const char *);
            if (str == NULL) {
                str = "(null)";
            }

            size_t len = strlen(str);
            if (flags.precision >= 0 && (size_t)flags.precision < len) {
                len = flags.precision;
            }

            // Padding before string (right-justified)
            if (!flags.left_justify && flags.width > (int)len) {
                int pad = flags.width - (int)len;
                for (int i = 0; i < pad; i++) {
                    *ptr++ = ' ';
                }
            }

            // Copy string
            for (size_t i = 0; i < len; i++) {
                *ptr++ = str[i];
            }

            // Padding after string (left-justified)
            if (flags.left_justify && flags.width > (int)len) {
                int pad = flags.width - (int)len;
                for (int i = 0; i < pad; i++) {
                    *ptr++ = ' ';
                }
            }
            break;
        }

        case 'd':
        case 'i': {
            // Properly handle signed integers
            int64_t value;
            int is_negative = 0;

            // Get value based on length modifier
            switch (flags.length_modifier) {
            case 'H': // char
                value = (signed char)va_arg(ap, int);
                break;
            case 'h': // short
                value = (short)va_arg(ap, int);
                break;
            case 'l': // long
                value = va_arg(ap, long);
                break;
            case 'L': // long long
                value = va_arg(ap, long long);
                break;
            case 'j': // intmax_t
                value = va_arg(ap, int64_t);
                break;
            case 'z': // size_t
                value = (int64_t)va_arg(ap, size_t);
                break;
            case 't': // ptrdiff_t
                value = (int64_t)va_arg(ap, long);
                break;
            default: // int
                value = va_arg(ap, int);
                break;
            }

            // Store if negative and convert to positive for uitoa
            if (value < 0) {
                is_negative = 1;
                value = -value;
            }

            char *num_str = temp_buffer;
            if (flags.precision == 0 && value == 0) {
                // Handle zero precision with zero value
                num_str[0] = '\0';
            } else {
                // Convert number to string
                uitoa((uint64_t)value, num_str, 10, 0);
            }

            size_t len = strlen(num_str);

            // Determine sign character properly
            char sign = 0;
            if (is_negative) {
                sign = '-';
            } else if (flags.always_sign) {
                sign = '+';
            } else if (flags.space_sign) {
                sign = ' ';
            }

            // Create final number string with sign
            char final_buffer[128];
            char *final_str = final_buffer;
            size_t final_len = len;

            if (sign) {
                *final_str++ = sign;
                final_len++;
            }

            memcpy(final_str, num_str, len + 1);
            final_str = final_buffer;

            // Handle precision padding (zeros after sign, before number)
            if (flags.precision > (int)len) {
                int pad = flags.precision - (int)len;
                char prec_buffer[128];
                char *pb = prec_buffer;

                if (sign) {
                    *pb++ = sign;
                }

                for (int i = 0; i < pad; i++) {
                    *pb++ = '0';
                }

                memcpy(pb, num_str, len + 1);
                final_str = prec_buffer;
                final_len = strlen(prec_buffer);
            }

            // Width padding
            if (!flags.left_justify && flags.width > (int)final_len) {
                int pad = flags.width - (int)final_len;
                char pad_char =
                    (flags.zero_pad && flags.precision < 0) ? '0' : ' ';

                // If zero padding with sign, print sign first
                if (pad_char == '0' && sign) {
                    *ptr++ = sign;
                    final_str++; // Skip sign in final_str
                    final_len--;
                }

                for (int i = 0; i < pad; i++) {
                    *ptr++ = pad_char;
                }
            }

            // Copy number
            for (size_t i = 0; i < final_len; i++) {
                *ptr++ = final_str[i];
            }

            // Padding after number
            if (flags.left_justify && flags.width > (int)final_len) {
                int pad = flags.width - (int)final_len;
                for (int i = 0; i < pad; i++) {
                    *ptr++ = ' ';
                }
            }
            break;
        }

        case 'u':
        case 'o':
        case 'x':
        case 'X':
        case 'p': {
            // Unsigned integer formats
            uint64_t value;
            int base;
            int uppercase = 0;

            // Handle pointer separately
            if (flags.specifier == 'p') {
                value = (uint64_t)va_arg(ap, void *);
                base = 16;
                flags.alternate_form = 1;
            } else {
                // Get value based on length modifier
                switch (flags.length_modifier) {
                case 'H': // unsigned char
                    value = (unsigned char)va_arg(ap, unsigned int);
                    break;
                case 'h': // unsigned short
                    value = (unsigned short)va_arg(ap, unsigned int);
                    break;
                case 'l': // unsigned long
                    value = va_arg(ap, unsigned long);
                    break;
                case 'L': // unsigned long long
                    value = va_arg(ap, unsigned long long);
                    break;
                case 'j': // uintmax_t
                    value = va_arg(ap, uint64_t);
                    break;
                case 'z': // size_t
                    value = va_arg(ap, size_t);
                    break;
                case 't': // ptrdiff_t
                    value = (uint64_t)va_arg(ap, long);
                    break;
                default: // unsigned int
                    value = va_arg(ap, unsigned int);
                    break;
                }

                // Determine base
                switch (flags.specifier) {
                case 'o':
                    base = 8;
                    break;
                case 'x':
                    base = 16;
                    break;
                case 'X':
                    base = 16;
                    uppercase = 1;
                    break;
                default:
                    base = 10;
                    break;
                }
            }

            // Convert number to string
            char *num_str = temp_buffer;
            if (flags.precision == 0 && value == 0 && flags.specifier != 'p') {
                // Handle zero precision with zero value
                num_str[0] = '\0';
            } else {
                uitoa(value, num_str, base, uppercase);
            }

            size_t len = strlen(num_str);

            // Handle alternate form prefix
            char prefix[3] = {0};
            int prefix_len = 0;

            if (flags.alternate_form && value != 0) {
                if (base == 8) {
                    prefix[0] = '0';
                    prefix_len = 1;
                } else if (base == 16) {
                    prefix[0] = '0';
                    prefix[1] = uppercase ? 'X' : 'x';
                    prefix_len = 2;
                }
            }

            // Handle precision padding
            if (flags.precision > (int)(len + prefix_len)) {
                int pad = flags.precision - (int)(len + prefix_len);
                char *new_str = temp_buffer + 64;
                for (int i = 0; i < pad; i++) {
                    new_str[i] = '0';
                }
                memcpy(new_str + pad, num_str, len + 1);
                num_str = new_str;
                len = strlen(num_str);
            }

            // Total length with prefix
            int total_len = (int)len + prefix_len;

            // Padding before number
            if (!flags.left_justify && flags.width > total_len) {
                int pad = flags.width - total_len;
                char pad_char =
                    (flags.zero_pad && flags.precision < 0) ? '0' : ' ';

                for (int i = 0; i < pad; i++) {
                    *ptr++ = pad_char;
                }
            }

            // Write prefix
            for (int i = 0; i < prefix_len; i++) {
                *ptr++ = prefix[i];
            }

            // Copy number
            for (size_t i = 0; i < len; i++) {
                *ptr++ = num_str[i];
            }

            // Padding after number
            if (flags.left_justify && flags.width > total_len) {
                int pad = flags.width - total_len;
                for (int i = 0; i < pad; i++) {
                    *ptr++ = ' ';
                }
            }
            break;
        }

        case 'f':
        case 'F': {
            // Floating point fixed notation
            double value = va_arg(ap, double);

            // Convert to string
            char *num_str = temp_buffer;
            ftoa(value,
                 num_str,
                 flags.precision,
                 (flags.specifier == 'F'),
                 flags.alternate_form);

            size_t len = strlen(num_str);

            // Handle sign for positive numbers
            char sign = 0;
            int skip_first = 0;

            // Check if the string already has a sign
            if (num_str[0] == '-') {
                sign = '-';
                skip_first = 1; // Skip the minus that's already in string
            } else if (num_str[0] == '+') {
                sign = '+';
                skip_first = 1;
            } else {
                // Add sign if needed for positive numbers
                if (flags.always_sign) {
                    sign = '+';
                } else if (flags.space_sign) {
                    sign = ' ';
                }
            }

            // Calculate effective length (without the sign we'll handle
            // separately)
            size_t effective_len = skip_first ? len - 1 : len;
            int total_len = (int)effective_len + (sign ? 1 : 0);

            // Padding before number
            if (!flags.left_justify && flags.width > total_len) {
                int pad = flags.width - total_len;
                char pad_char = (flags.zero_pad) ? '0' : ' ';

                // Special handling for zero padding with sign
                if (pad_char == '0' && sign) {
                    *ptr++ = sign;
                    sign = 0; // Sign already printed
                }

                for (int i = 0; i < pad; i++) {
                    *ptr++ = pad_char;
                }
            }

            // Add sign if not already printed
            if (sign) {
                *ptr++ = sign;
            }

            // Copy number (skip sign if already in string)
            const char *copy_start = skip_first ? num_str + 1 : num_str;
            size_t copy_len = skip_first ? len - 1 : len;

            for (size_t i = 0; i < copy_len; i++) {
                *ptr++ = copy_start[i];
            }

            // Padding after number
            if (flags.left_justify && flags.width > total_len) {
                int pad = flags.width - total_len;
                for (int i = 0; i < pad; i++) {
                    *ptr++ = ' ';
                }
            }
            break;
        }

        case 'e':
        case 'E': {
            // Floating point scientific notation
            double value = va_arg(ap, double);

            // Convert to string
            char *num_str = temp_buffer;
            etoa(value, num_str, flags.precision, (flags.specifier == 'E'));

            size_t len = strlen(num_str);

            // Handle sign for positive numbers
            char sign = 0;
            int skip_first = 0;

            // Check if the string already has a sign
            if (num_str[0] == '-') {
                sign = '-';
                skip_first = 1;
            } else if (num_str[0] == '+') {
                sign = '+';
                skip_first = 1;
            } else {
                if (flags.always_sign) {
                    sign = '+';
                } else if (flags.space_sign) {
                    sign = ' ';
                }
            }

            size_t effective_len = skip_first ? len - 1 : len;
            int total_len = (int)effective_len + (sign ? 1 : 0);

            // Padding before number
            if (!flags.left_justify && flags.width > total_len) {
                int pad = flags.width - total_len;
                char pad_char = (flags.zero_pad) ? '0' : ' ';

                if (pad_char == '0' && sign) {
                    *ptr++ = sign;
                    sign = 0;
                }

                for (int i = 0; i < pad; i++) {
                    *ptr++ = pad_char;
                }
            }

            if (sign) {
                *ptr++ = sign;
            }

            const char *copy_start = skip_first ? num_str + 1 : num_str;
            size_t copy_len = skip_first ? len - 1 : len;

            for (size_t i = 0; i < copy_len; i++) {
                *ptr++ = copy_start[i];
            }

            if (flags.left_justify && flags.width > total_len) {
                int pad = flags.width - total_len;
                for (int i = 0; i < pad; i++) {
                    *ptr++ = ' ';
                }
            }
            break;
        }

        case 'g':
        case 'G': {
            // Floating point general notation
            double value = va_arg(ap, double);

            // Convert to string
            char *num_str = temp_buffer;
            gtoa(value, num_str, flags.precision, (flags.specifier == 'G'));

            size_t len = strlen(num_str);

            // Handle sign for positive numbers
            char sign = 0;
            int skip_first = 0;

            // Check if the string already has a sign
            if (num_str[0] == '-') {
                sign = '-';
                skip_first = 1;
            } else if (num_str[0] == '+') {
                sign = '+';
                skip_first = 1;
            } else {
                if (flags.always_sign) {
                    sign = '+';
                } else if (flags.space_sign) {
                    sign = ' ';
                }
            }

            size_t effective_len = skip_first ? len - 1 : len;
            int total_len = (int)effective_len + (sign ? 1 : 0);

            // Padding before number
            if (!flags.left_justify && flags.width > total_len) {
                int pad = flags.width - total_len;
                char pad_char = (flags.zero_pad) ? '0' : ' ';

                if (pad_char == '0' && sign) {
                    *ptr++ = sign;
                    sign = 0;
                }

                for (int i = 0; i < pad; i++) {
                    *ptr++ = pad_char;
                }
            }

            if (sign) {
                *ptr++ = sign;
            }

            const char *copy_start = skip_first ? num_str + 1 : num_str;
            size_t copy_len = skip_first ? len - 1 : len;

            for (size_t i = 0; i < copy_len; i++) {
                *ptr++ = copy_start[i];
            }

            if (flags.left_justify && flags.width > total_len) {
                int pad = flags.width - total_len;
                for (int i = 0; i < pad; i++) {
                    *ptr++ = ' ';
                }
            }
            break;
        }

        case 'n': {
            // Store number of characters written so far
            int *count_ptr = va_arg(ap, int *);
            *count_ptr = (int)(ptr - buffer);
            break;
        }

        default: {
            // Unknown specifier, just copy the format
            format -= consumed;
            *ptr++ = *format++;
            break;
        }
        }
    }

    *ptr = '\0';
    return (int)(ptr - buffer);
}

/*
 * Complete printf() function with full format support
 */
__attribute__((format(printf, 1, 2))) //
ssize_t printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);

    // Buffer for formatted output
    char buffer[4096];

    // Format to buffer
    int len = format_to_buffer(buffer, format, ap);

    va_end(ap);

    // Write to stdout
    ssize_t written = write(STDOUT_FILENO, buffer, len);
    if (written < 0) {
        exit(EXIT_FAILURE);
    }

    return written;
}

/*
 * Test program with floating point
 */
int main(int argc, char **argv) {
    // Test basic formats
    printf("Basic tests:\n");
    printf("String: %s\n", "Hello World");
    printf("Character: %c\n", 'A');
    printf("Integer: %d\n", 123);
    printf("Negative: %d\n", -456);
    printf("Unsigned: %u\n", 789);
    printf("Octal: %o\n", 255);
    printf("Hex lowercase: %x\n", 255);
    printf("Hex uppercase: %X\n", 255);
    printf("Pointer: %p\n", (void *)main);
    printf("NULL str: %s\n", (char *)NULL);

    // Test flags
    printf("\nFlag tests:\n");
    printf("Width 10: |%10d|\n", 123);
    printf("Left justify: |%-10d|\n", 123);
    printf("Zero pad: |%010d|\n", 123);
    printf("Sign: |%+d|\n", 123);
    printf("Space: |% d|\n", 123);
    printf("Alternate hex: %#x\n", 255);
    printf("Alternate octal: %#o\n", 255);

    // Test precision
    printf("\nPrecision tests:\n");
    printf("Precision 5: %.5d\n", 123);
    printf("Precision 2: %.2d\n", 123);
    printf("Precision 0: %.0d\n", 0);
    printf("String precision: %.5s\n", "Hello World");

    // Test combinations
    printf("\nCombination tests:\n");
    printf("|%10.5d|\n", 123);
    printf("|%-10.5d|\n", 123);
    printf("|%+10.5d|\n", 123);
    printf("|%+-10.5d|\n", 123);

    // Test floating point formats
    printf("\nFloating point tests:\n");

    // Basic f format
    printf("Fixed (default): %f\n", 123.456789);
    printf("Fixed (precision 2): %.2f\n", 123.456789);
    printf("Fixed (precision 0): %.0f\n", 123.456789);
    printf("Fixed (width 10): |%10f|\n", 123.456789);
    printf("Fixed (width 10, precision 2): |%10.2f|\n", 123.456789);
    printf("Fixed (negative): %f\n", -123.456789);
    printf("Fixed (zero): %f\n", 0.0);
    printf("Fixed (small): %f\n", 0.000123456);
    printf("Fixed (large): %f\n", 123456789.0);

    // Scientific notation
    printf("\nScientific notation:\n");
    printf("Scientific (default): %e\n", 123.456789);
    printf("Scientific (precision 2): %.2e\n", 123.456789);
    printf("Scientific (uppercase): %E\n", 123.456789);
    printf("Scientific (negative): %e\n", -123.456789);
    printf("Scientific (small): %e\n", 0.000123456);
    printf("Scientific (large): %e\n", 123456789.0);

    // General format
    printf("\nGeneral format:\n");
    printf("General (default): %g\n", 123.456789);
    printf("General (precision 4): %.4g\n", 123.456789);
    printf("General (uppercase): %G\n", 123.456789);
    printf("General (small): %g\n", 0.000123456);
    printf("General (very small): %g\n", 0.000000123456);
    printf("General (large): %g\n", 123456789.0);
    printf("General (very large): %g\n", 123456789012345.0);

    // Special floating point values
    printf("\nSpecial values:\n");
    double inf = 1.0 / 0.0;
    double nan = 0.0 / 0.0;
    printf("Infinity: %f\n", inf);
    printf("Negative infinity: %f\n", -inf);
    printf("NaN: %f\n", nan);
    printf("Infinity (scientific): %e\n", inf);
    printf("NaN (uppercase): %F\n", nan);

    // Test alternate form
    printf("\nAlternate form:\n");
    printf("Alternate (f): %#.0f\n", 123.0);
    printf("Alternate (g): %#g\n", 123.0);
    printf("Alternate (g with decimal): %#g\n", 123.456);

    // Test with signs
    printf("\nSign tests for floats:\n");
    printf("Always sign: %+f\n", 123.456);
    printf("Space sign: % f\n", 123.456);
    printf("Always sign (negative): %+f\n", -123.456);

    // Test argument passing
    printf("\nArgument tests:\n");
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    // Bonus: test input
    char buffer[1024];
    while (1) {

        printf("Input something: ");
        ssize_t len = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (len <= 0) { // Handle EOF
            return EXIT_FAILURE;
        }

        // delete new line
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';

        } else if ((size_t)len >= sizeof(buffer)) {
            buffer[sizeof(buffer) - 1] = '\0';

        } else {
            buffer[len] = '\0';
        }

        if (len > 1) {
            printf("Your input is '%s'\n", buffer);
            break;
        }

        printf("Please input something!!\n");
    }

    return EXIT_SUCCESS;
}

// setup stack
void _start_main(long *stack) {
    int argc = (int)stack[0];
    char **argv = (char **)(stack + 1);
    // char **envp = argv + argc + 1;
    int ret = main(argc, argv);
    exit(ret);
}

__attribute__((naked, noreturn)) void _start(void) {
    asm(                   //
        "    mov x0, sp\n" // sp = stack
        "    bl _start_main\n"

    );
}
