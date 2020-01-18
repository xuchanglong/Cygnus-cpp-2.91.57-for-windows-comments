/*
 * Copyright (c) 1996-1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

/* NOTE: This is an internal header file, included by other STL headers.
 *   You should not attempt to use it directly.
 */

#ifndef __SGI_STL_INTERNAL_ALLOC_H
#define __SGI_STL_INTERNAL_ALLOC_H

//	Sun 的 cc 编译器
#ifdef __SUNPRO_CC
#define __PRIVATE public
// Extra access restrictions prevent us from really making some things
// private.
#else
#define __PRIVATE private
#endif

#ifdef __STL_STATIC_TEMPLATE_MEMBER_BUG
#define __USE_MALLOC
#endif

// This implements some standard node allocators.  These are
// NOT the same as the allocators in the C++ draft standard or in
// in the original STL.  They do not encapsulate different pointer
// types; indeed we assume that there is only one pointer type.
// The allocation primitives are intended to allocate individual objects,
// not larger arenas as with the original STL allocators.

#if 0
#include <new>
#define __THROW_BAD_ALLOC throw bad_alloc
#elif !defined(__THROW_BAD_ALLOC)
#include <iostream.h>
#define __THROW_BAD_ALLOC            \
    cerr << "out of memory" << endl; \
    exit(1)
#endif

#ifndef __ALLOC
#define __ALLOC alloc
#endif
#ifdef __STL_WIN32THREADS
#include <windows.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef __RESTRICT
#define __RESTRICT
#endif

#if !defined(__STL_PTHREADS) && !defined(_NOTHREADS) && !defined(__STL_SGI_THREADS) && !defined(__STL_WIN32THREADS)
#define _NOTHREADS
#endif

#ifdef __STL_PTHREADS
// POSIX Threads
// This is dubious, since this is likely to be a high contention
// lock.   Performance may not be adequate.
#include <pthread.h>
#define __NODE_ALLOCATOR_LOCK \
    if (threads)              \
    pthread_mutex_lock(&__node_allocator_lock)
#define __NODE_ALLOCATOR_UNLOCK \
    if (threads)                \
    pthread_mutex_unlock(&__node_allocator_lock)
#define __NODE_ALLOCATOR_THREADS true
#define __VOLATILE volatile // Needed at -O3 on SGI
#endif
#ifdef __STL_WIN32THREADS
// The lock needs to be initialized by constructing an allocator
// objects of the right type.  We do that here explicitly for alloc.
#define __NODE_ALLOCATOR_LOCK \
    EnterCriticalSection(&__node_allocator_lock)
#define __NODE_ALLOCATOR_UNLOCK \
    LeaveCriticalSection(&__node_allocator_lock)
#define __NODE_ALLOCATOR_THREADS true
#define __VOLATILE volatile // may not be needed
#endif                      /* WIN32THREADS */
#ifdef __STL_SGI_THREADS
// This should work without threads, with sproc threads, or with
// pthreads.  It is suboptimal in all cases.
// It is unlikely to even compile on nonSGI machines.

extern "C"
{
    extern int __us_rsthread_malloc;
}
// The above is copied from malloc.h.  Including <malloc.h>
// would be cleaner but fails with certain levels of standard
// conformance.
#define __NODE_ALLOCATOR_LOCK            \
    if (threads && __us_rsthread_malloc) \
    {                                    \
        __lock(&__node_allocator_lock);  \
    }
#define __NODE_ALLOCATOR_UNLOCK           \
    if (threads && __us_rsthread_malloc)  \
    {                                     \
        __unlock(&__node_allocator_lock); \
    }
#define __NODE_ALLOCATOR_THREADS true
#define __VOLATILE volatile // Needed at -O3 on SGI
#endif
#ifdef _NOTHREADS
//  Thread-unsafe
#define __NODE_ALLOCATOR_LOCK
#define __NODE_ALLOCATOR_UNLOCK
#define __NODE_ALLOCATOR_THREADS false
#define __VOLATILE
#endif

__STL_BEGIN_NAMESPACE

#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma set woff 1174
#endif

// Malloc-based allocator.  Typically slower than default alloc below.
// Typically thread-safe and more storage efficient.
#ifdef __STL_STATIC_TEMPLATE_MEMBER_BUG
#ifdef __DECLARE_GLOBALS_HERE
void (*__malloc_alloc_oom_handler)() = 0;
// g++ 2.7.2 does not handle static template data members.
#else
extern void (*__malloc_alloc_oom_handler)();
#endif
#endif

// ================================== 一级配置器 ================================== //

// malloc-based allocator，一级配置器。
// 注意，无“template 型别参数”，inst 无用。
template <int inst>
class __malloc_alloc_template
{

private:
    // 以下函数用于处理内存不足的情况。
    // oom，代表 out of memory。
    static void *oom_malloc(size_t);

    static void *oom_realloc(void *, size_t);

#ifndef __STL_STATIC_TEMPLATE_MEMBER_BUG
    static void (*__malloc_alloc_oom_handler)();
#endif

public:
    static void *allocate(size_t n)
    {
        // 一级分配器直接使用malloc()。
        void *result = malloc(n);
        // 若内存不够用，则改用oom_malloc()
        if (0 == result)
            result = oom_malloc(n);
        return result;
    }

    static void deallocate(void *p, size_t /* n */)
    {
        // 一级分配器直接使用free()。
        free(p);
    }

    static void *reallocate(void *p, size_t /* old_sz */, size_t new_sz)
    {
        // 一级分配器直接使用realloc()。
        void *result = realloc(p, new_sz);
        // 若内存不足，则改用realloc()。
        if (0 == result)
            result = oom_realloc(p, new_sz);
        return result;
    }

    // 仿真C++的set_new_handler()，即：通过该函数制定自己的 Out of memory handler。
    // 函数等价如下：
    //              using TYPE_F =  void (*)();
    //              TYPE_F set_malloc_handler(TYPE_F f)
    // 即：形参是 void (*)() 且返回值也是 void (*)() 。
    // https://blog.csdn.net/charles1e/article/details/51620673
    static void (*set_malloc_handler(void (*f)()))()
    {
        void (*old)() = __malloc_alloc_oom_handler;
        __malloc_alloc_oom_handler = f;
        return (old);
    }
};

// malloc_alloc out-of-memory handling
// C++ new-handler 内存不足时的处理例程。
// 需要由开发者进行指定。
#ifndef __STL_STATIC_TEMPLATE_MEMBER_BUG
template <int inst>
void (*__malloc_alloc_template<inst>::__malloc_alloc_oom_handler)() = 0;
#endif

template <int inst>
void *__malloc_alloc_template<inst>::oom_malloc(size_t n)
{
    void (*my_malloc_handler)();
    void *result;
    // 不断的尝试释放、配置、再释放、再配置……
    for (;;)
    {
        my_malloc_handler = __malloc_alloc_oom_handler;
        if (0 == my_malloc_handler)
        {
            __THROW_BAD_ALLOC;
        }
        // 调用处理例程，企图释放内存。
        (*my_malloc_handler)();
        // 再次尝试配置内存。
        result = malloc(n);
        if (result)
            return (result);
    }
}

template <int inst>
void *__malloc_alloc_template<inst>::oom_realloc(void *p, size_t n)
{
    void (*my_malloc_handler)();
    void *result;

    for (;;)
    {
        my_malloc_handler = __malloc_alloc_oom_handler;
        if (0 == my_malloc_handler)
        {
            __THROW_BAD_ALLOC;
        }
        (*my_malloc_handler)();
        result = realloc(p, n);
        if (result)
            return (result);
    }
}

typedef __malloc_alloc_template<0> malloc_alloc;

/*
 *	SGI STL 容器都使用 simple_alloc 作为接口。
 */
template <class T, class Alloc>
class simple_alloc
{

public:
    static T *allocate(size_t n)
    {
        return 0 == n ? 0 : (T *)Alloc::allocate(n * sizeof(T));
    }
    static T *allocate(void)
    {
        return (T *)Alloc::allocate(sizeof(T));
    }
    static void deallocate(T *p, size_t n)
    {
        if (0 != n)
            Alloc::deallocate(p, n * sizeof(T));
    }
    static void deallocate(T *p)
    {
        Alloc::deallocate(p, sizeof(T));
    }
};

// Allocator adaptor to check size arguments for debugging.
// Reports errors using assert.  Checking can be disabled with
// NDEBUG, but it's far better to just use the underlying allocator
// instead when no checking is desired.
// There is some evidence that this can confuse Purify.
template <class Alloc>
class debug_alloc
{

private:
    enum
    {
        extra = 8
    }; // Size of space used to store size.  Note
    // that this must be large enough to preserve
    // alignment.

public:
    static void *allocate(size_t n)
    {
        char *result = (char *)Alloc::allocate(n + extra);
        *(size_t *)result = n;
        return result + extra;
    }

    static void deallocate(void *p, size_t n)
    {
        char *real_p = (char *)p - extra;
        assert(*(size_t *)real_p == n);
        Alloc::deallocate(real_p, n + extra);
    }

    static void *reallocate(void *p, size_t old_sz, size_t new_sz)
    {
        char *real_p = (char *)p - extra;
        assert(*(size_t *)real_p == old_sz);
        char *result = (char *)
            Alloc::reallocate(real_p, old_sz + extra, new_sz + extra);
        *(size_t *)result = new_sz;
        return result + extra;
    }
};

/*
 *	若定义了 __USE_MALLOC ，则 alloc 采用一级配置器。
 *	否则采用二级的配置器。
 */
#ifdef __USE_MALLOC

typedef malloc_alloc alloc;
typedef malloc_alloc single_client_alloc;

#else

// ================================== 二级配置器 ================================== //
// 默认节点分配。
// Default node allocator.
// With a reasonable compiler, this should be roughly as fast as the
// original STL class-specific allocators, but with less fragmentation.
// Default_alloc_template parameters are experimental and MAY
// DISAPPEAR in the future.  Clients should just use alloc for now.
//
// Important implementation properties:
// 1. If the client request an object of size > __MAX_BYTES, the resulting
//    object will be obtained directly from malloc.
// 2. In all other cases, we allocate an object of size exactly
//    ROUND_UP(requested_size).  Thus the client has enough size
//    information that we can return the object to the proper free list
//    without permanently losing part of the object.
//

// The first template parameter specifies whether more than one thread
// may use this allocator.  It is safe to allocate an object from
// one instance of a default_alloc and deallocate it with another
// one.  This effectively transfers its ownership to the second one.
// This may have undesirable effects on reference locality.
// The second parameter is unreferenced and serves only to allow the
// creation of multiple default_alloc instances.
// Node that containers built on different allocator instances have
// different types, limiting the utility of this approach.
#ifdef __SUNPRO_CC
// breaks if we make these template class members:
// 小型区块的上调边界
enum
{
    __ALIGN = 8
};
// 小型区块的上限
enum
{
    __MAX_BYTES = 128
};
// free-lists 个数
enum
{
    __NFREELISTS = __MAX_BYTES / __ALIGN
};
#endif

// 无 “ template 型别参数” ，且第二个参数无用。
// 第一个参数用于多线程环境下。
template <bool threads, int inst>
class __default_alloc_template
{

private:
    // Really we should use static const int x = N
    // instead of enum { x = N }, but few compilers accept the former.
#ifndef __SUNPRO_CC
    enum
    {
        __ALIGN = 8
    };
    enum
    {
        __MAX_BYTES = 128
    };
    // 分配了16个链表（0 ~ 15），节点内存大小分别是8、16、24、32、……、128。
    enum
    {
        __NFREELISTS = __MAX_BYTES / __ALIGN
    };
#endif
    // 将 bytes 上调至8的倍数，用于对齐使用。
    static size_t ROUND_UP(size_t bytes)
    {
        return (((bytes) + __ALIGN - 1) & ~(__ALIGN - 1));
    }
    __PRIVATE :
        // free-list 的节点构造。
        union obj {
        // 指向下一个节点的首地址。
        union obj *free_list_link;
        // 指向本节点的首地址。
        char client_data[1]; /* The client sees this. */
    };

private:
#ifdef __SUNPRO_CC
    static obj *__VOLATILE free_list[];
    // Specifying a size results in duplicate def for 4.1
#else
    // 16个free-lists。这些链表始终存放空闲块。
    static obj *__VOLATILE free_list[__NFREELISTS];
#endif
    // 根据待分配的内存，返回第 n 号 free-lists 。n 从 0 算起。
    static size_t FREELIST_INDEX(size_t bytes)
    {
        // 返回对应的0-15的值。
        // 当 bytes 为10时，对齐之后，实际分配的内存块大小为16，应该用1号链表。
        // 那么 (bytes + __ALIGN - 1  ) / __ALIGN, 就是为了计算其所属链表的号，
        // 但是该号是从 0 开始的，故再减一。
        return (((bytes) + __ALIGN - 1) / __ALIGN - 1);
    }

    // 返回大小为n的区块，并且可能加入大小为 n 的其他区块到 free list 中。
    // Returns an object of size n, and optionally adds to size n free list.
    static void *refill(size_t n);

    // 分配一大块内存，可容纳 nobjs 个大小为 size 的内存。
    // 如果申请这么多内存不方便 nobjs 可能会降低
    // Allocates a chunk for nobjs of size "size".  nobjs may be reduced
    // if it is inconvenient to allocate the requested number.
    static char *chunk_alloc(size_t size, int &nobjs);

    // Chunk allocation state.
    // 内存池的起始位置，只在 chunk_alloc() 中变化。
    static char *start_free;
    // 内存池的终止位置，只在 chunk_alloc() 中变化。
    static char *end_free;
    static size_t heap_size;

#ifdef __STL_SGI_THREADS
    static volatile unsigned long __node_allocator_lock;
    static void __lock(volatile unsigned long *);
    static inline void __unlock(volatile unsigned long *);
#endif

#ifdef __STL_PTHREADS
    static pthread_mutex_t __node_allocator_lock;
#endif

#ifdef __STL_WIN32THREADS
    static CRITICAL_SECTION __node_allocator_lock;
    static bool __node_allocator_lock_initialized;

public:
    __default_alloc_template()
    {
        // This assumes the first constructor is called before threads
        // are started.
        if (!__node_allocator_lock_initialized)
        {
            InitializeCriticalSection(&__node_allocator_lock);
            __node_allocator_lock_initialized = true;
        }
    }

private:
#endif

    class lock
    {
    public:
        lock()
        {
            __NODE_ALLOCATOR_LOCK;
        }
        ~lock()
        {
            __NODE_ALLOCATOR_UNLOCK;
        }
    };
    friend class lock;

public:
    /* n must be > 0      */
    static void *allocate(size_t n)
    {
        obj *__VOLATILE *my_free_list;
        obj *__RESTRICT result;
        // 若 n > 128，则调用一级分配器。
        if (n > (size_t)__MAX_BYTES)
        {
            return (malloc_alloc::allocate(n));
        }
        // 寻找 16 个 free lists 中适当的一个。
        my_free_list = free_list + FREELIST_INDEX(n);
        // Acquire the lock here with a constructor call.
        // This ensures that it is released in exit or during stack
        // unwinding.
#ifndef _NOTHREADS
        /*REFERENCED*/
        lock lock_instance;
#endif
        // *my_free_list 值是存放该链表中下一个可用的内存块的首地址。
        result = *my_free_list;
        // 若没有可用的 free list，则准备重新填充 free list 。
        if (result == 0)
        {
            void *r = refill(ROUND_UP(n));
            return r;
        }
        // 调整 freelist。即：始终从链表的首位置提取内存块。
        *my_free_list = result->free_list_link;
        return (result);
    };

    /* p may not be 0 */
    static void deallocate(void *p, size_t n)
    {
        //	存储要释放的内存的节点
        obj *q = (obj *)p;
        //	存储制定节点的首地址的地址。
        obj *__VOLATILE *my_free_list;
        //	调用一级配置器进行内存回收。
        if (n > (size_t)__MAX_BYTES)
        {
            malloc_alloc::deallocate(p, n);
            return;
        }
        //	寻找对应的 freelist
        my_free_list = free_list + FREELIST_INDEX(n);
        // acquire lock
#ifndef _NOTHREADS
        /*REFERENCED*/
        lock lock_instance;
#endif /* _NOTHREADS */
        //	调整 freelist，回收区块。始终将空闲节点放入链表的首位置。
        q->free_list_link = *my_free_list;
        *my_free_list = q;
        // lock is released here
    }

    static void *reallocate(void *p, size_t old_sz, size_t new_sz);
};

typedef __default_alloc_template<__NODE_ALLOCATOR_THREADS, 0> alloc;
typedef __default_alloc_template<false, 0> single_client_alloc;

// 我们申请一个大个内存，内存大小为 size * nobjs 。
/* We allocate memory in large chunks in order to avoid fragmenting     */
/* the malloc heap too much.                                            */
/* We assume that size is properly aligned.                             */
/* We hold the allocation lock.                                         */
template <bool threads, int inst>
char * // size 为单个区块的大小，nobjs 为所有的区块个数。
__default_alloc_template<threads, inst>::chunk_alloc(size_t size, int &nobjs)
{
    char *result;
    // 计算总共的区块大小。
    size_t total_bytes = size * nobjs;
    // 内存池剩余量。
    size_t bytes_left = end_free - start_free;
    // 若剩余的内存足够大，则直接返回 start_free。
    if (bytes_left >= total_bytes)
    {
        result = start_free;
        start_free += total_bytes;
        return (result);
    }
    // 内存池剩余空间不能满足要求，但是能够供应至少一个区块。
    else if (bytes_left >= size)
    {
        nobjs = bytes_left / size;
        total_bytes = size * nobjs;
        result = start_free;
        start_free += total_bytes;
        return (result);
    }
    // 内存池剩余空间连一个内存块也供应不了
    else
    {
        size_t bytes_to_get = 2 * total_bytes + ROUND_UP(heap_size >> 4);
        // Try to make use of the left-over piece.
        //	试图将内存池中剩余内存重新划到别的 free list 中，以充分利用。
        if (bytes_left > 0)
        {
            //	内存池中还有一些零头，先分配给适当的 free list。
            //	首先先找到合适 free list。
            obj *__VOLATILE *my_free_list =
                free_list + FREELIST_INDEX(bytes_left);
            //	调整 free list，将剩余空间编入。
            ((obj *)start_free)->free_list_link = *my_free_list;
            *my_free_list = (obj *)start_free;
        }
        // 配置heap空间，来补充内存池。
        start_free = (char *)malloc(bytes_to_get);
        // heap 不足，malloc 失败。
        if (0 == start_free)
        {
            int i;
            obj *__VOLATILE *my_free_list, *p;
            // Try to make do with what we have.  That can't
            // hurt.  We do not try smaller requests, since that tends
            // to result in disaster on multi-process machines.
            // 尝试从其他链表中找到尚存的并且足够大的区块。
            for (i = size; i <= __MAX_BYTES; i += __ALIGN)
            {
                my_free_list = free_list + FREELIST_INDEX(i);
                p = *my_free_list;
                //	free list 中尚有未用的区块。
                if (0 != p)
                {
                    //	调整 free list ，以释出未用的区块。
                    *my_free_list = p->free_list_link;
                    start_free = (char *)p;
                    end_free = start_free + i;
                    //	递归调用自己，调正 nobjs。
                    return (chunk_alloc(size, nobjs));
                    // Any leftover piece will eventually make it to the
                    // right free list.
                    //	注意，任何残余的内存都将编入 free list 中。
                }
            }
            //	山穷水尽了，找不到任何内存块了。
            end_free = 0; // In case of exception.
            //	调用一级配置器。
            start_free = (char *)malloc_alloc::allocate(bytes_to_get);
            // This should either throw an
            // exception or remedy the situation.  Thus we assume it
            // succeeded.
        }
        heap_size += bytes_to_get;
        end_free = start_free + bytes_to_get;
        //	递归自己，修正nobjs。
        return (chunk_alloc(size, nobjs));
    }
}

//	返回20个区块大小为n的大内存，并且添加到单个区块为 n 的 free list 中。
/* Returns an object of size n, and optionally adds to size n free list.*/
//	我们假定 n 是对齐之后大小。
/* We assume that n is properly aligned.                                */
/* We hold the allocation lock.                                         */
template <bool threads, int inst>
void *__default_alloc_template<threads, inst>::refill(size_t n)
{
    int nobjs = 20;
    // 申请20个，每个大小为 n 的内存块
    char *chunk = chunk_alloc(n, nobjs);
    // 存储制定 free list 中存储的内容，即：区块的节点的首地址。
    obj *__VOLATILE *my_free_list;
    // 存储返回的结果。
    obj *result;
    // 当前节点的首地址和下一个节点的首地址。
    obj *current_obj, *next_obj;
    int i;
    // 若只用1个区块，那分配的这个大区块直接给调用者用。
    if (1 == nobjs)
        return (chunk);
    // 获得指定链表的首地址的地址。
    my_free_list = free_list + FREELIST_INDEX(n);

    /* Build free list in chunk */
    // 以下在chunk空间内建立free list
    // result 作为返回值。
    result = (obj *)chunk;
    // 以下引到 free list 指向新配置的空间。
    *my_free_list = next_obj = (obj *)(chunk + n);
    // 以下将 free list 的各个节点串接起来。
    for (i = 1;; i++)
    {
        current_obj = next_obj;
        next_obj = (obj *)((char *)next_obj + n);
        if (nobjs - 1 == i)
        {
            current_obj->free_list_link = 0;
            break;
        }
        else
        {
            current_obj->free_list_link = next_obj;
        }
    }
    return (result);
}

template <bool threads, int inst>
void *
__default_alloc_template<threads, inst>::reallocate(void *p,
                                                    size_t old_sz,
                                                    size_t new_sz)
{
    void *result;
    size_t copy_sz;

    if (old_sz > (size_t)__MAX_BYTES && new_sz > (size_t)__MAX_BYTES)
    {
        return (realloc(p, new_sz));
    }
    if (ROUND_UP(old_sz) == ROUND_UP(new_sz))
        return (p);
    result = allocate(new_sz);
    copy_sz = new_sz > old_sz ? old_sz : new_sz;
    memcpy(result, p, copy_sz);
    deallocate(p, old_sz);
    return (result);
}

#ifdef __STL_PTHREADS
template <bool threads, int inst>
pthread_mutex_t
    __default_alloc_template<threads, inst>::__node_allocator_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifdef __STL_WIN32THREADS
template <bool threads, int inst>
CRITICAL_SECTION
    __default_alloc_template<threads, inst>::__node_allocator_lock;

template <bool threads, int inst>
bool
    __default_alloc_template<threads, inst>::__node_allocator_lock_initialized = false;
#endif

#ifdef __STL_SGI_THREADS
__STL_END_NAMESPACE
#include <mutex.h>
#include <time.h>
__STL_BEGIN_NAMESPACE
// Somewhat generic lock implementations.  We need only test-and-set
// and some way to sleep.  These should work with both SGI pthreads
// and sproc threads.  They may be useful on other systems.
template <bool threads, int inst>
volatile unsigned long
    __default_alloc_template<threads, inst>::__node_allocator_lock = 0;

#if __mips < 3 || !(defined(_ABIN32) || defined(_ABI64)) || defined(__GNUC__)
#define __test_and_set(l, v) test_and_set(l, v)
#endif

template <bool threads, int inst>
void __default_alloc_template<threads, inst>::__lock(volatile unsigned long *lock)
{
    const unsigned low_spin_max = 30;    // spin cycles if we suspect uniprocessor
    const unsigned high_spin_max = 1000; // spin cycles for multiprocessor
    static unsigned spin_max = low_spin_max;
    unsigned my_spin_max;
    static unsigned last_spins = 0;
    unsigned my_last_spins;
    static struct timespec ts = {0, 1000};
    unsigned junk;
#define __ALLOC_PAUSE \
    junk *= junk;     \
    junk *= junk;     \
    junk *= junk;     \
    junk *= junk
    int i;

    if (!__test_and_set((unsigned long *)lock, 1))
    {
        return;
    }
    my_spin_max = spin_max;
    my_last_spins = last_spins;
    for (i = 0; i < my_spin_max; i++)
    {
        if (i < my_last_spins / 2 || *lock)
        {
            __ALLOC_PAUSE;
            continue;
        }
        if (!__test_and_set((unsigned long *)lock, 1))
        {
            // got it!
            // Spinning worked.  Thus we're probably not being scheduled
            // against the other process with which we were contending.
            // Thus it makes sense to spin longer the next time.
            last_spins = i;
            spin_max = high_spin_max;
            return;
        }
    }
    // We are probably being scheduled against the other process.  Sleep.
    spin_max = low_spin_max;
    for (;;)
    {
        if (!__test_and_set((unsigned long *)lock, 1))
        {
            return;
        }
        nanosleep(&ts, 0);
    }
}

template <bool threads, int inst>
inline void
__default_alloc_template<threads, inst>::__unlock(volatile unsigned long *lock)
{
#if defined(__GNUC__) && __mips >= 3
    asm("sync");
    *lock = 0;
#elif __mips >= 3 && (defined(_ABIN32) || defined(_ABI64))
    __lock_release(lock);
#else
    *lock = 0;
    // This is not sufficient on many multiprocessors, since
    // writes to protected variables and the lock may be reordered.
#endif
}
#endif

template <bool threads, int inst>
char *__default_alloc_template<threads, inst>::start_free = 0;

template <bool threads, int inst>
char *__default_alloc_template<threads, inst>::end_free = 0;

template <bool threads, int inst>
size_t __default_alloc_template<threads, inst>::heap_size = 0;

template <bool threads, int inst>
__default_alloc_template<threads, inst>::obj *__VOLATILE
    __default_alloc_template<threads, inst>::free_list[
#ifdef __SUNPRO_CC
        __NFREELISTS
#else
        __default_alloc_template<threads, inst>::__NFREELISTS
#endif
] = {
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
};
// The 16 zeros are necessary to make version 4.1 of the SunPro
// compiler happy.  Otherwise it appears to allocate too little
// space for the array.

#ifdef __STL_WIN32THREADS
// Create one to get critical section initialized.
// We do this onece per file, but only the first constructor
// does anything.
static alloc __node_allocator_dummy_instance;
#endif

#endif /* ! __USE_MALLOC */

#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma reset woff 1174
#endif

__STL_END_NAMESPACE

#undef __PRIVATE

#endif /* __SGI_STL_INTERNAL_ALLOC_H */

// Local Variables:
// mode:C++
// End:
