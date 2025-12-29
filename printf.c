// printf without any header or library only work in linux aarch64

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

// Define ssize_t and size_t since we're not including standard headers
typedef unsigned long int size_t;
typedef long ssize_t;

// define va_list
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
 * Simplified printf() function
 * NOTE: This is a very basic version that only prints strings
 * A full printf() would need to handle format specifiers like %d, %s, etc.
 */

__attribute__((format(printf, 1, 2))) //
ssize_t printf(const char *format, ...) {
    // just use va_list like a normal va_list
    va_list ap;

    va_start(ap, format);

    ssize_t total = 0;

    while (*format) {
        // NOTE: just implement %s not all format
        if (*format == '%' && *(format + 1) == 's') {
            format += 2;

            const char *s = va_arg(ap, char *);
            if (s) {
                ssize_t len = strlen(s);
                ssize_t written = write(STDOUT_FILENO, s, len);
                // On write error, abort program
                if (written < 0) {
                    va_end(ap);
                    exit(EXIT_FAILURE);
                }
                total += len;
            }
        } else {
            ssize_t written = write(STDOUT_FILENO, format, 1);
            if (written < 0) {
                va_end(ap);
                exit(EXIT_FAILURE);
            }
            total += 1;
            format++;
        }
    }

    va_end(ap);
    return total;
}

int main(int argc, char **argv) {

    for (int i = 1; i < argc; i++) {
        printf("%s\n", argv[i]);
    }

    printf("Hello World\n");

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
    asm(
        "    mov x0, sp\n" // sp = stack
        "    bl _start_main\n"

    );
}
