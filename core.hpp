/// A simple core library for C++
/// It implements custom allocators and better primitives data structures, which
/// take advantage of these allocators

#pragma once
#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/// ------------------
/// Defer
/// source: https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/
/// ------------------

template <typename F> struct privDefer {
    F f;
    privDefer(F f) : f(f) {
    }
    ~privDefer() {
        f();
    }
};

template <typename F> privDefer<F> defer_func(F f) {
    return privDefer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x) DEFER_2(x, __COUNTER__)
#define defer(code) auto DEFER_3(_defer_) = defer_func([&]() { code; })

/// ------------------
/// Type aliasses
/// ------------------

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using usize = size_t;
using isize = ptrdiff_t;

/// ------------------
/// Assert
/// ------------------

#ifndef LIBFUZZER
#define core_debug_trap() __builtin_trap()
#else
#define core_debug_trap() std::abort()
#endif

#ifdef NO_ASSERTS
#define core_assert_msg(...) (void)0
#endif

#ifndef core_assert_msg
#define core_assert_msg(cond, msg, ...)                                        \
    do {                                                                       \
        if (!(cond)) {                                                         \
            core_assert_handler("Assertion Failure", #cond, __FILE__,          \
                                (i64)__LINE__, msg, ##__VA_ARGS__);            \
            core_debug_trap();                                                 \
        }                                                                      \
    } while (0)
#endif

#ifndef core_assert
#define core_assert(cond) core_assert_msg(cond, NULL)
#endif

inline void core_assert_handler(char const* prefix, char const* condition,
                                char const* file, i32 line, const char* msg,
                                ...) {
    fprintf(stderr, "%s(%d): %s: ", file, line, prefix);
    if (condition)
        fprintf(stderr, "`%s` ", condition);
    if (msg) {
        va_list va;
        va_start(va, msg);
        vfprintf(stderr, msg, va);
        va_end(va);
    }
    fprintf(stderr, "\n");
}

/// ------------------
/// Memory allocation
/// ------------------

const isize DEFAULT_ALIGNMENT = alignof(max_align_t);

enum class AllocationMode : u8 {
    Alloc,
    Free,
    Resize,
};

using AllocatorProc = void* (*)(void* allocator, AllocationMode mode,
                                isize size, isize alignment, void* old_memory,
                                isize old_size);

struct Allocator {
    AllocatorProc alloc;
    void* data;
};

template <typename T>
inline T* core_alloc(Allocator allocator, isize count = 1,
                     isize alignment = alignof(T)) {
    core_assert_msg((alignment & (alignment - 1)) == 0,
                    "Alignment must be a power of 2");
    return (T*)allocator.alloc(allocator.data, AllocationMode::Alloc,
                               count * sizeof(T), alignment, nullptr, 0);
}

inline void core_free(Allocator allocator, void* memory) {
    allocator.alloc(allocator.data, AllocationMode::Free, 0, 0, memory, 0);
}

template <typename T>
inline T* core_realloc(Allocator allocator, void* memory, isize old_size,
                       isize new_size, isize alignment = alignof(T)) {
    return (T*)allocator.alloc(allocator.data, AllocationMode::Resize, new_size,
                               alignment, memory, old_size);
}

// malloc based allocator

static void* c_allocator_proc(void* allocator, AllocationMode mode, isize size,
                              isize alignment, void* old_memory,
                              isize old_size) {
    core_assert(allocator == nullptr);
    core_assert((usize)alignment <= alignof(max_align_t));
    switch (mode) {
    case AllocationMode::Alloc: {
        void* data = calloc(size, 1);
        core_assert(data != nullptr);
        return data;
    }
    case AllocationMode::Free: {
        free(old_memory);
        return nullptr;
    }
    case AllocationMode::Resize: {
        void* data = realloc(old_memory, size);
        core_assert(data != nullptr);
        memset((u8*)data + old_size, 0, size - old_size);
        return data;
    }
    }
}

constexpr Allocator c_allocator() {
    return Allocator{
        .alloc = c_allocator_proc,
        .data = nullptr,
    };
}

/// ------------------
/// Slice
/// ------------------

template <typename T> struct Slice {
    T* data;
    isize size;

    T& operator[](isize index) {
        core_assert(index >= 0);
        core_assert(index < this->size);
        return data[index];
    }

    const T& operator[](isize index) const {
        core_assert(index >= 0);
        core_assert(index < this->size);
        return data[index];
    }

    // Iterator support for range-based for loops
    T* begin() {
        return data;
    }
    T* end() {
        return data + size;
    }

    // For const iteration
    const T* begin() const {
        return data;
    }
    const T* end() const {
        return data + size;
    }

    // For const iteration with non-const objects
    const T* cbegin() const {
        return data;
    }
    const T* cend() const {
        return data + size;
    }
};

template <typename T, isize N>
inline Slice<T> slice_from_inline_alloc(const T (&data)[N], Allocator alloc) {
    T* new_data = core_alloc<T>(alloc, N);
    memcpy(new_data, data, sizeof(T) * N);
    return Slice<T>{new_data, N};
}

template <typename T>
inline Slice<T> slice_subslice(Slice<T> slice, isize start, isize end) {
    core_assert(start >= 0);
    core_assert(end <= slice.size);
    core_assert(start < end);
    return Slice<T>{slice.data + start, end - start};
}

template <typename R> inline R* slice_cast_raw(Slice<u8> slice) {
    core_assert(slice.size == sizeof(R));
    return (R*)slice.data;
}

template <typename T> inline Slice<T> slice_from_parts(T* data, isize size) {
    return Slice<T>{data, size};
}

template <typename T> inline Slice<T> slice_make(isize size, Allocator alloc) {
    T* data = core_alloc<T>(alloc, size);
    return Slice<T>{data, size};
}

template <typename T>
inline Slice<T> slice_copy(Slice<T> slice, Allocator allocator) {
    T* data = core_alloc<T>(allocator, slice.size);
    memcpy(data, slice.data, sizeof(T) * slice.size);
    return Slice<T>{data, slice.size};
}

template <typename T>
inline Slice<T>* slice_copy_alloc(Slice<T> slice, Allocator alloc) {
    Slice<T>* data = core_alloc<Slice<T>>(alloc);
    data->data = core_alloc<T>(alloc, slice.size);
    memcpy(data->data, slice.data, sizeof(T) * slice.size);
    data->size = slice.size;
    return data;
}

template <typename T>
inline void slice_fill(Slice<T> slice, T value, isize start = 0,
                       isize end = -1) {
    core_assert(start >= 0);
    if (end == -1) {
        end = slice.size;
    }
    core_assert(end <= slice.size);

    for (isize i = start; i < end; i++) {
        slice[i] = value;
    }
}

template <typename T> inline void slice_clear_to_zero(Slice<T> slice) {
    core_assert(slice.data != nullptr);
    core_assert(slice.size > 0);
    memset(slice.data, 0, sizeof(T) * slice.size);
}

template <typename T> inline bool slice_contains(Slice<T> slice, T value) {
    for (isize i = 0; i < slice.size; i++) {
        if (slice[i] == value) {
            return true;
        }
    }
    return false;
}

/// ------------------
/// Arena
/// ------------------

struct Arena {
    Slice<u8> data;
    isize offset;
};

inline void arena_init(Arena* arena, Slice<u8> data) {
    core_assert(arena != nullptr);
    core_assert(data.data != nullptr);
    core_assert(data.size > 0);

    arena->data = data;
    arena->offset = 0;
}

inline Arena arena_make(Slice<u8> data) {
    Arena arena;
    arena_init(&arena, data);
    return arena;
}

inline u8* arena_alloc(Arena* arena, isize size,
                       isize alignment = DEFAULT_ALIGNMENT) {
    core_assert(arena != nullptr);
    core_assert(arena->data.data != nullptr);
    core_assert(arena->offset >= 0);
    core_assert(arena->offset <= arena->data.size);

    isize aligned_offset = (arena->offset + alignment - 1) & ~(alignment - 1);
    isize new_offset = aligned_offset + size;
    core_assert_msg(new_offset <= arena->data.size, "Arena out of memory");
    arena->offset = new_offset;

    void* data = arena->data.data + aligned_offset;
    return (u8*)data;
}

inline u8* arena_realloc(Arena* arena, u8* old_memory, isize old_size,
                         isize new_size, isize alignment = DEFAULT_ALIGNMENT) {
    core_assert(arena != nullptr);
    core_assert(arena->data.data != nullptr);
    core_assert(arena->offset >= 0);
    core_assert(arena->offset <= arena->data.size);
    core_assert(new_size > 0);
    core_assert(((usize)old_memory & (alignment - 1)) == 0);

    if (old_memory == nullptr) {
        return arena_alloc(arena, new_size, alignment);
    }

    // Check if we are the last allocation
    if ((u8*)old_memory == arena->data.data + arena->offset - old_size) {
        core_assert_msg(arena->offset - old_size + new_size <= arena->data.size,
                        "Arena out of memory");

        arena->offset += new_size - old_size;
        core_assert(arena->offset >= 0);

        // Clear the memory if the size is decreased
        if (new_size < old_size) {
            memset(old_memory + new_size, 0, old_size - new_size);
        }

        return old_memory;
    }

    u8* new_memory = arena_alloc(arena, new_size, alignment);
    memcpy(new_memory, old_memory, old_size);
    return new_memory;
}

static void* arena_alloc_proc(void* allocator, AllocationMode mode, isize size,
                              isize alignment, void* old_memory,
                              isize old_size) {
    Arena* arena = (Arena*)allocator;

    switch (mode) {
    case AllocationMode::Alloc: {
        return arena_alloc(arena, size, alignment);
    }
    case AllocationMode::Free: {
        return nullptr;
    }
    case AllocationMode::Resize: {
        return arena_realloc(arena, (u8*)old_memory, old_size, size, alignment);
    }
    }
}

inline Allocator arena_allocator(Arena* arena) {
    return Allocator{
        .alloc = arena_alloc_proc,
        .data = arena,
    };
}

inline void arena_reset(Arena* arena) {
    core_assert(arena != nullptr);
    core_assert(arena->data.data != nullptr);
    core_assert(arena->offset >= 0);
    core_assert(arena->offset <= arena->data.size);

    arena->offset = 0;
    slice_clear_to_zero(arena->data);
}

/// ------------------
/// Dynamic Arena
/// ------------------

struct MemoryBlock {
    u8* data;
    isize capacity;
    isize size;
    MemoryBlock* prev;
};

struct DynamicArena {
    Allocator alloc;
    MemoryBlock* current;
    isize block_size_min;
};

inline MemoryBlock* memory_block_create(isize size, Allocator alloc) {
    MemoryBlock* block =
        core_alloc<MemoryBlock>(alloc, (sizeof(MemoryBlock) + size));
    block->data = (u8*)(block + 1);
    block->size = 0;
    block->capacity = size;
    block->prev = nullptr;

    return block;
}

inline void dynamic_arena_init(DynamicArena* arena, isize block_size_min,
                               Allocator alloc = c_allocator()) {
    arena->block_size_min = block_size_min;

    arena->alloc = alloc;
    MemoryBlock* block = memory_block_create(block_size_min, arena->alloc);
    arena->current = block;
}

inline DynamicArena dynamic_arena_make(isize block_size_min = 4 * 1024 * 1024,
                                       Allocator alloc = c_allocator()) {
    DynamicArena arena;
    arena.alloc = alloc;
    dynamic_arena_init(&arena, block_size_min);
    return arena;
}

inline u8* dynamic_arena_alloc(DynamicArena* arena, isize size,
                               isize alignment = DEFAULT_ALIGNMENT) {
    u8* result = arena->current->data + arena->current->size;

    // Align forward to minimum alignment
    result = (u8*)((isize)(result + alignment - 1) & ~(alignment - 1));

    isize new_size = (result + size) - arena->current->data;

    if (new_size <= arena->current->capacity) {
        arena->current->size = new_size;
        memset(result, 0, size);
        return result;
    }

    isize new_capacity = std::max(arena->block_size_min, size);
    MemoryBlock* new_block = memory_block_create(new_capacity, arena->alloc);

    new_block->prev = arena->current;
    arena->current = new_block;
    arena->current->size = size;

    memset(new_block->data, 0, size);
    return new_block->data;
}

inline u8* dynamic_arena_realloc(DynamicArena* arena, u8* old_memory,
                                 isize old_size, isize new_size,
                                 isize alignment = DEFAULT_ALIGNMENT) {
    core_assert(arena != nullptr);
    core_assert(arena->current != nullptr);
    core_assert(old_memory != nullptr);
    core_assert(old_size > 0);
    core_assert(new_size > 0);
    core_assert(((usize)old_memory & (alignment - 1)) == 0);

    // Check if we are the last allocation
    if (old_memory == arena->current->data + arena->current->size - old_size) {
        isize new_block_size =
            (u8*)old_memory - arena->current->data + new_size;

        if (new_block_size > arena->current->capacity) {
            u8* new_memory = dynamic_arena_alloc(arena, new_size, alignment);
            memcpy(new_memory, old_memory, old_size);
            return new_memory;
        }

        arena->current->size =
            (u8*)old_memory - arena->current->data + new_size;
        core_assert(arena->current->size >= 0);

        if (new_size < old_size) {
            memset(old_memory + new_size, 0, old_size - new_size);
        }

        return old_memory;
    }

    u8* new_memory = dynamic_arena_alloc(arena, new_size, alignment);
    memcpy(new_memory, old_memory, old_size);
    return new_memory;
}

inline void dynamic_arena_free(DynamicArena* arena) {
    MemoryBlock* block = arena->current;
    while (block) {
        MemoryBlock* prev = block->prev;
        core_free(arena->alloc, block);
        block = prev;
    }

    arena->current = nullptr;
}

inline void dynamic_arena_reset(DynamicArena* arena) {
    core_assert(arena != nullptr);
    core_assert(arena->current != nullptr);

    MemoryBlock* block = arena->current;
    while (block) {
        if (block->prev == nullptr) {
            block->size = 0;
            memset(block->data, 0, block->capacity);
            arena->current = block;
            return;
        }

        MemoryBlock* prev = block->prev;
        core_free(arena->alloc, block);
        block = prev;
    }
}

static void* dynamic_arena_alloc_proc(void* allocator, AllocationMode mode,
                                      isize size, isize alignment,
                                      void* old_memory, isize old_size) {
    DynamicArena* arena = (DynamicArena*)allocator;

    switch (mode) {
    case AllocationMode::Alloc: {
        return dynamic_arena_alloc(arena, size, alignment);
    }
    case AllocationMode::Free: {
        return nullptr;
    }
    case AllocationMode::Resize: {
        return dynamic_arena_realloc(arena, (u8*)old_memory, old_size, size,
                                     alignment);
    }
    }
}

inline Allocator dynamic_arena_allocator(DynamicArena* arena) {
    return Allocator{
        .alloc = dynamic_arena_alloc_proc,
        .data = arena,
    };
}

inline isize dynamic_arena_get_size(DynamicArena* arena) {
    isize size = 0;
    MemoryBlock* block = arena->current;
    while (block) {
        size += block->size;
        block = block->prev;
    }

    return size;
}

/// ------------------
/// Array List
/// ------------------

template <typename T> struct Array {
    Allocator alloc;
    Slice<T> items;
    isize capacity;
};

template <typename T>
inline void array_init(Array<T>* list, Allocator alloc, isize capacity = 0) {
    core_assert(list != nullptr);
    core_assert(alloc.alloc != nullptr);
    core_assert(capacity > 0);

    list->alloc = alloc;
    if (capacity > 0) {
        list->items.data = core_alloc<T>(alloc, capacity);
        list->items.size = 0;
    }
    list->capacity = capacity;
}

template <typename T>
inline Array<T> array_make(Allocator alloc, isize capacity = 0) {
    Array<T> list;
    array_list_init(&list, alloc, capacity);
    return list;
}

template <typename T> inline void array_push(Array<T>* array, T item) {
    core_assert(array != nullptr);
    core_assert(array->alloc.alloc != nullptr);
    core_assert(array->items.data != nullptr);
    core_assert(array->items.size >= 0);
    core_assert(array->items.size <= array->capacity);

    if (array->items.size + 1 > array->capacity) {
        isize new_capacity = std::max(array->capacity * 2, (isize)4);
        array->items.data = core_realloc<T>(array->alloc, array->items.data,
                                            array->capacity * sizeof(T),
                                            new_capacity * sizeof(T));
        array->capacity = new_capacity;
    }

    array->items[array->items.size++] = item;
}

template <typename T>
inline void array_push_slice(Array<T>* array, Slice<T> slice) {
    for (const T item : slice) {
        array_push(array, item);
    }
}

template <typename T> inline T array_pop(Array<T>* array) {
    core_assert(array != nullptr);
    core_assert(array->alloc.alloc != nullptr);
    core_assert(array->items.data != nullptr);
    core_assert(array->items.size > 0);
    core_assert(array->items.size <= array->capacity);

    T item = array->items[array->items.size - 1];
    array->items.size--;
    return item;
}

template <typename T> inline void array_clear(Array<T>* array) {
    core_assert(array != nullptr);
    core_assert(array->alloc.alloc != nullptr);
    core_assert(array->items.data != nullptr);
    core_assert(array->items.size <= array->capacity);

    array->items.size = 0;
}

template <typename T>
inline T array_remove_at_unordered(Array<T>* array, isize index) {
    core_assert_msg(index >= 0, "%ld < 0", index);
    core_assert_msg(index < array->items.size, "%ld >= %ld", index,
                    array->items.size);

    T value = array->items[index];
    array->items[index] = array->items[array->size - 1];
    array->size -= 1;

    return value;
}

template <typename T>
inline void array_insert(Array<T>* array, isize index, T value) {
    core_assert_msg(index >= 0, "%ld < 0", index);
    core_assert_msg(index <= array->size, "%ld > %ld", index, array->size);

    array_push(array, value);

    for (isize i = array->size - 1; i > index; i--) {
        array->data[i] = array->data[i - 1];
    }

    array->data[index] = value;
}
