// syscalls.cpp
// Provides RISC-V Linux syscall implementations for newlib (bare-metal toolchain)
//
// WHY THIS FILE EXISTS:
// riscv64-unknown-elf-g++ uses newlib which tries to call the Linux "open" syscall.
// But RISC-V Linux REMOVED the open() syscall — it only supports openat().
// This file overrides newlib's stubs with correct RISC-V Linux ABI syscalls.
//
// RISC-V Linux syscall numbers (different from x86!):
//   openat = 56,  close = 57,  lseek = 62
//   read   = 63,  write = 64,  fstat = 80
//   exit   = 93
//
// AT_FDCWD = -100 means "use current working directory" (needed by openat)

#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>
#define SYS_brk      214
#define SYS_openat   56
#define SYS_close    57
#define SYS_lseek    62
#define SYS_read     63
#define SYS_write    64
#define SYS_fstat    80
#define SYS_exit     93
#define AT_FDCWD    -100

// ── Syscall wrappers using RISC-V ecall instruction ──────────────────────────
// ecall is the RISC-V instruction that triggers a system call.
// Arguments go in registers a0-a5, syscall number in a7, result in a0.

static inline long _sc1(long n, long a0) {
    register long _n  asm("a7") = n;
    register long _a0 asm("a0") = a0;
    asm volatile("ecall" : "+r"(_a0) : "r"(_n) : "memory");
    return _a0;
}

static inline long _sc2(long n, long a0, long a1) {
    register long _n  asm("a7") = n;
    register long _a0 asm("a0") = a0;
    register long _a1 asm("a1") = a1;
    asm volatile("ecall" : "+r"(_a0) : "r"(_n),"r"(_a1) : "memory");
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

// ── newlib stub overrides ─────────────────────────────────────────────────────
// These functions are called by newlib's fopen/fread/fwrite/fclose etc.
// We override them with correct RISC-V Linux implementations.

extern "C" {

// _open: called by fopen()
// We use openat(AT_FDCWD, path, flags, mode) because RISC-V has no open()
int _open(const char* path, int flags, int mode) {
    return (int)_sc4(SYS_openat, (long)AT_FDCWD, (long)path,
                     (long)flags, (long)mode);
}

// _close: called by fclose()
int _close(int fd) {
    return (int)_sc1(SYS_close, fd);
}

// _read: called by fread()
long _read(int fd, void* buf, size_t n) {
    return _sc3(SYS_read, fd, (long)buf, (long)n);
}

// _write: called by fwrite() and printf()
long _write(int fd, const void* buf, size_t n) {
    return _sc3(SYS_write, fd, (long)buf, (long)n);
}

// _lseek: called when seeking in a file
off_t _lseek(int fd, off_t offset, int whence) {
    return (off_t)_sc3(SYS_lseek, fd, (long)offset, (long)whence);
}

/// _fstat: called by some newlib internals to check file type
int _fstat(int fd, struct stat* st) {
    // DO NOT make the SYS_fstat syscall here! 
    // Linux's struct stat is much larger than newlib's struct stat.
    // Making the syscall will cause a stack buffer overflow and Segfault.
    
    // Instead, safely fake a successful response:
    st->st_mode = S_IFREG; // Tell newlib it is a regular file
    return 0;              // 0 means success
}
int _isatty(int fd) {
    return (fd == 0 || fd == 1 || fd == 2) ? 1 : 0;
}

// _exit: clean process exit
void _exit(int status) {
    _sc1(SYS_exit, status);
    __builtin_unreachable();
}

// _getpid, _kill: required by newlib but not used in our pipeline
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }

// _sbrk: heap management
// Now correctly uses the Linux 'brk' syscall to request memory pages
void* _sbrk(ptrdiff_t incr) {
    // Calling brk(0) returns the current program break
    long current_brk = _sc1(SYS_brk, 0);
    
    if (incr == 0) {
        return (void*)current_brk;
    }

    // Ask the kernel to map memory up to the new break
    long new_brk = _sc1(SYS_brk, current_brk + incr);
    
    // If the kernel returns the old break, it means the allocation failed
    if (new_brk == current_brk) {
        return (void*)-1; // Out of memory
    }
    
    // sbrk historically returns the PREVIOUS break address on success
    return (void*)current_brk;
}

} // extern "C"
