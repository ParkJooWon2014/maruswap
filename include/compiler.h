#ifndef __DMS_COMPILER_H__
#define __DMS_COMPILER_H__

// Refer to the compiler.h in kernl.

#define HAVE_BUILTIN_CTZ 1
#define HAVE_BUILTIN_EXPECT 1

#define prefetch(x)		__builtin_prefetch(x)
#define prefetchw(x)	__builtin_prefetch(x,1)

#define likely(x)		__builtin_expect(!!(x),1)
#define unlikely(x)		__builtin_expect(!!(x),0)

// Variable attributes
#define __unused__		__attribute__((unused))
#define __packed__		__attribute__((__packed__))

#define barrier() __asm__ __volatile__("": : :"memory")

#define BITS_PER_LONG 64

#define CONFIG_SMP
#define CONFIG_X86_64

#ifdef CONFIG_SMP
#define LOCK_PREFIX_HERE \
		".section .smp_locks,\"a\"\n"	\
		".balign 4\n"			\
		".long 671f - .\n" /* offset */	\
		".previous\n"			\
		"671:"

#define LOCK_PREFIX LOCK_PREFIX_HERE "\n\tlock; "

#else /* ! CONFIG_SMP */
#define LOCK_PREFIX_HERE ""
#define LOCK_PREFIX ""
#endif

/* Are two types/vars the same type (ignoring qualifiers)? */
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE, MEMBER)  __compiler_offsetof(TYPE, MEMBER)
#else
#define offsetof(TYPE, MEMBER)  ((size_t)&((TYPE *)0)->MEMBER)
#endif

#endif
