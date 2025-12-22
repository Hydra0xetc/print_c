// print without any header or library only work in linux aarch64

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
typedef __SIZE_TYPE__ size_t;
typedef long ssize_t;

// File descriptor for standard output is 1 thats way 1> or 2> for stderr
// see: https://en.wikipedia.org/wiki/Standard_streams
// for more information
// NOTE: stdin is 0, stdout is 2, stderr is 3
#define STDOUT_FILENO 1

// System call number for write() on ARM64 Linux
// You can find this in: /usr/include/asm-generic/unistd.h
// or check with: grep "__NR_write" /usr/include/asm*/unistd.h
#define __NR_write 64

// Exit status codes
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

/*
 * Inline assembly implementation of write() system call
 * System call convention for ARM64:
 * - x0: first argument (file descriptor)
 * - x1: second argument (buffer pointer)
 * - x2: third argument (count)
 * - x8: system call number
 * - svc 0: supervisor call (like int 0x80 on x86)
 * Return value is in x0
 */
static inline ssize_t write(int fd, const void *buf, ssize_t count) {

    // Load arguments into registers following ARM64 calling convention
    register long x0 asm("x0") = fd;         // File descriptor
    register long x1 asm("x1") = (long)buf;  // Buffer address
    register long x2 asm("x2") = count;      // Number of bytes to write
    register long x8 asm("x8") = __NR_write; // System call number

    // Inline assembly for system call
    asm volatile(
        "svc 0"    // Make supervisor call (system call)
        : "+r"(x0) // Output: x0 (return value) is both input and output
        : "r"(x1), "r"(x2), "r"(x8) // Input: x1, x2, x8 registers
        : "memory"                  // Clobber: memory may be modified
    );

    return x0; // Return value from system call (negative for error)
}

/*
 * Custom strlen() implementation
 * Since we're not using standard library, we need to implement this ourselves
 * Returns the length of a null-terminated string
 */
size_t strlen(const char *s) {
    const char *p = s; // Start at beginning of string

    // Loop until we find null terminator
    while (*p != '\0') {
        p++;
    }

    // Calculate length: end pointer minus start pointer
    return (size_t)(p - s);
}

/*
 * Simplified print() function
 * NOTE: This is a very basic version that only prints strings
 * A full printf() would need to handle format specifiers like %d, %s, etc.
 */

ssize_t print(const char *format, ...) {

    // For now, ignore variadic arguments (...) - this just prints the string
    // In a real implementation, you'd parse the format string here

    // Call write() system call
    ssize_t ret = write(STDOUT_FILENO, format, strlen(format));

    // Return negative value on error, number of bytes written on success
    return (ret < 0) ? -1 : ret;
}

int main(void) {
    print("Hello World\n");
    return EXIT_SUCCESS;
}
