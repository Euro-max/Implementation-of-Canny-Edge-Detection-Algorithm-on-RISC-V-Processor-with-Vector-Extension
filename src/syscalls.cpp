// syscalls.cpp
// Provides RISC-V Linux syscall implementations for newlib (bare-metal toolchain)
//
// WHY THIS FILE EXISTS:
// riscv64-unknown-elf-g++ uses newlib which tries to call the Linux "open" syscall.
// But RISC-V Linux REMOVED the open() syscall — it only supports openat().
// This file overrides newlib's stubs with correct RISC-V Linux ABI syscalls.

#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>

// Syscall Numbers
#define SYS_brk      214
#define SYS_openat   56
#define SYS_close    57
#define SYS_lseek    62
#define SYS_read     63
#define SYS_write    64
#define SYS_fstat    80
#define SYS_exit     93
#define AT_FDCWD    -100

// Flag Translation Constants (Newlib vs Linux)
#define LINUX_O_CREAT   0x0040
#define LINUX_O_TRUNC   0x0200
#define LINUX_O_APPEND  0x0400
#define NEWLIB_O_CREAT  0x0200
#define NEWLIB_O_TRUNC  0x0400
#define NEWLIB_O_APPEND 0x0008

// ── Syscall wrappers ──────────────────────────────────────────────────────────
static inline long _sc1(long n, long a0) {
    register long _n  asm("a7") = n;
    register long _a0 asm("a0") = a0;
    asm volatile("ecall" : "+r"(_a0) : "r"(_n) : "memory");
    return _a0;
}

static inline long _sc3(long n, long a0, long a1, long a2) {
    register long _n  asm("a7") = n;
    register long _a0 asm("a0") = a0;
    register long _a1 asm("a1") = a1;
    register long _a2 asm("a2") = a2;
    asm volatile("ecall" : "+r"(_a0) : "r"(_n),"r"(_a1),"r"(_a2) : "memory");
    return _a0;
}

static inline long _sc4(long n, long a0, long a1, long a2, long a3) {
    register long _n  asm("a7") = n;
    register long _a0 asm("a0") = a0;
    register long _a1 asm("a1") = a1;
    register long _a2 asm("a2") = a2;
    register long _a3 asm("a3") = a3;
    asm volatile("ecall" : "+r"(_a0) : "r"(_n),"r"(_a1),"r"(_a2),"r"(_a3) : "memory");
    return _a0;
}

extern "C" {

// _open: Fixed with flag translation
int _open(const char* path, int flags, int mode) {
    int linux_flags = flags & 3; 
    
    if (flags & NEWLIB_O_CREAT)  linux_flags |= LINUX_O_CREAT;
    if (flags & NEWLIB_O_TRUNC)  linux_flags |= LINUX_O_TRUNC;
    if (flags & NEWLIB_O_APPEND) linux_flags |= LINUX_O_APPEND;

    return (int)_sc4(SYS_openat, (long)AT_FDCWD, (long)path,
                     (long)linux_flags, (long)mode);
}

int _close(int fd) { return (int)_sc1(SYS_close, fd); }
long _read(int fd, void* buf, size_t n) { return _sc3(SYS_read, fd, (long)buf, (long)n); }
long _write(int fd, const void* buf, size_t n) { return _sc3(SYS_write, fd, (long)buf, (long)n); }
off_t _lseek(int fd, off_t offset, int whence) { return (off_t)_sc3(SYS_lseek, fd, (long)offset, (long)whence); }

int _fstat(int fd, struct stat* st) {
    st->st_mode = S_IFREG; 
    return 0; 
}

int _isatty(int fd) { return (fd == 0 || fd == 1 || fd == 2) ? 1 : 0; }
void _exit(int status) { _sc1(SYS_exit, status); __builtin_unreachable(); }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }

void* _sbrk(ptrdiff_t incr) {
    long current_brk = _sc1(SYS_brk, 0);
    if (incr == 0) return (void*)current_brk;
    long new_brk = _sc1(SYS_brk, current_brk + incr);
    if (new_brk == current_brk) return (void*)-1;
    return (void*)current_brk;
}

} // extern "C"