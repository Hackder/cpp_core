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
#include <iostream>
#include <unordered_map>
#include <unordered_set>

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
/// Result
/// ------------------

template <typename T, typename E> struct Result {
    union {
        T value;
        E error;
    };
    bool is_ok;

    Result(T value, bool is_ok) : value(value), is_ok(is_ok) {
    }

    Result(E error, bool is_ok) : error(error), is_ok(is_ok) {
    }
};

#define result_ok(result) {result, true}

#define result_err(result) {result, false}

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
__attribute__((malloc)) __attribute__((returns_nonnull)) inline T*
core_alloc(Allocator allocator, isize count = 1, isize alignment = alignof(T)) {
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

// Malloc based allocator
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
inline Slice<T> slice_subslice(Slice<T> slice, isize start, isize count) {
    core_assert(start >= 0);
    core_assert(start + count <= slice.size);
    core_assert(count >= 0);
    return Slice<T>{slice.data + start, count};
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
    core_assert(end >= 0);
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

template <typename T> inline bool slice_equals(Slice<T> a, Slice<T> b) {
    if (a.size != b.size) {
        return false;
    }

    if (a.data == b.data) {
        return true;
    }

    return memcmp(a.data, b.data, a.size) == 0;
}

template <typename T> inline isize slice_index_of(Slice<T> slice, T value) {
    for (isize i = 0; i < slice.size; i++) {
        if (slice[i] == value) {
            return i;
        }
    }
    return -1;
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

    slice_clear_to_zero(data);
    arena->data = data;
    arena->offset = 0;
}

inline Arena arena_make(Slice<u8> data) {
    Arena arena;
    arena_init(&arena, data);
    return arena;
}

__attribute__((malloc)) __attribute__((returns_nonnull)) inline u8*
arena_alloc(Arena* arena, isize size, isize alignment = DEFAULT_ALIGNMENT) {
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

const isize DEFAULT_BLOCK_SIZE_MIN = 4 * 1024 * 1024; // 4 MB

inline void dynamic_arena_init(DynamicArena* arena,
                               isize block_size_min = DEFAULT_BLOCK_SIZE_MIN,
                               Allocator alloc = c_allocator()) {
    arena->block_size_min = block_size_min;

    arena->alloc = alloc;
    MemoryBlock* block = memory_block_create(block_size_min, arena->alloc);
    arena->current = block;
}

inline DynamicArena
dynamic_arena_make(isize block_size_min = DEFAULT_BLOCK_SIZE_MIN,
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

inline Allocator
create_temporary_storage(isize default_block_size = DEFAULT_BLOCK_SIZE_MIN) {
    DynamicArena dynamic_arena =
        dynamic_arena_make(default_block_size, c_allocator());
    return dynamic_arena_allocator(&dynamic_arena);
}

/// ------------------
/// Strings
/// ------------------

struct String {
    const char* data = nullptr;
    isize size = 0;

    const char& operator[](isize index) {
        core_assert_msg(index >= 0, "%ld < 0", index);
        core_assert_msg(index < size, "%ld >= %ld", index, size);
        return data[index];
    }
};

inline bool operator==(String a, String b) {
    if (a.size != b.size) {
        return false;
    }

    if (a.data == b.data) {
        return true;
    }

    return memcmp(a.data, b.data, a.size) == 0;
}

inline bool operator!=(String a, String b) {
    return !(a == b);
}

inline bool operator==(String a, const char* b) {
    isize b_size = (isize)strlen(b);
    if (a.size != b_size) {
        return false;
    }

    return memcmp(a.data, b, a.size) == 0;
}

inline bool operator!=(String a, const char* b) {
    return !(a == b);
}

// Only used to allow gtest to print strings
inline std::ostream& operator<<(std::ostream& os, String str) {
    for (isize i = 0; i < str.size; i++) {
        os << str.data[i];
    }

    return os;
}

struct rune {
    i32 codepoint;

    inline bool operator==(const rune& other) const {
        return codepoint == other.codepoint;
    }

    inline bool operator!=(const rune& other) const {
        return codepoint != other.codepoint;
    }
};

struct RuneDetails {
    rune value;
    isize size;
};

inline RuneDetails cstr_read_utf8_codepoint(const char* cstr) {
    unsigned char c = *cstr;
    i32 codepoint = 0;
    isize len = 1;
    if (c < 0x80) {
        codepoint = c;
    } else if ((c & 0xE0) == 0xC0) {
        codepoint = ((c & 0x1F) << 6) | (cstr[1] & 0x3F);
        len = 2;
    } else if ((c & 0xF0) == 0xE0) {
        codepoint =
            ((c & 0x0F) << 12) | ((cstr[1] & 0x3F) << 6) | (cstr[2] & 0x3F);
        len = 3;
    } else if ((c & 0xF8) == 0xF0) {
        codepoint = ((c & 0x07) << 18) | ((cstr[1] & 0x3F) << 12) |
                    ((cstr[2] & 0x3F) << 6) | (cstr[3] & 0x3F);
        len = 4;
    }
    return RuneDetails{.value = {codepoint}, .size = len};
}

inline rune rune_from_cstr(const char* cstr) {
    RuneDetails codepoint = cstr_read_utf8_codepoint(cstr);
    core_assert_msg(cstr[codepoint.size] == '\0',
                    "This should only be used for one rune strings");
    return codepoint.value;
}

inline void rune_to_cstr(rune r, char buffer[5]) {
    if (r.codepoint < 0x80) {
        buffer[0] = (char)r.codepoint;
        buffer[1] = '\0';
    } else if (r.codepoint < 0x800) {
        buffer[0] = (char)(0xC0 | (r.codepoint >> 6));
        buffer[1] = (char)(0x80 | (r.codepoint & 0x3F));
        buffer[2] = '\0';
    } else if (r.codepoint < 0x10000) {
        buffer[0] = (char)(0xE0 | (r.codepoint >> 12));
        buffer[1] = (char)(0x80 | ((r.codepoint >> 6) & 0x3F));
        buffer[2] = (char)(0x80 | (r.codepoint & 0x3F));
        buffer[3] = '\0';
    } else {
        buffer[0] = (char)(0xF0 | (r.codepoint >> 18));
        buffer[1] = (char)(0x80 | ((r.codepoint >> 12) & 0x3F));
        buffer[2] = (char)(0x80 | ((r.codepoint >> 6) & 0x3F));
        buffer[3] = (char)(0x80 | (r.codepoint & 0x3F));
        buffer[4] = '\0';
    }
}

inline bool operator==(const rune& a, const char* b) {
    return a.codepoint == rune_from_cstr(b).codepoint;
}

inline bool operator!=(const rune& a, const char* b) {
    return !(a == b);
}

inline bool operator==(const char* a, const rune& b) {
    return b == a;
}

inline bool operator!=(const char* a, const rune& b) {
    return !(b == a);
}

inline bool operator==(const rune& a, const char b) {
    return a.codepoint == (i32)b;
}

inline bool operator!=(const rune& a, const char b) {
    return !(a == b);
}

inline bool operator==(const char b, const rune& a) {
    return a == b;
}

inline bool operator!=(const char b, const rune& a) {
    return !(a == b);
}

inline String string_from_cstr(const char* cstr) {
    return String{.data = cstr, .size = (isize)strlen(cstr)};
}

inline String string_from_cstr_alloc(const char* cstr, Allocator alloc) {
    isize size = (isize)strlen(cstr);
    char* data = core_alloc<char>(alloc, size + 1);
    memcpy(data, cstr, size);
    data[size] = '\0';

    return String{.data = data, .size = size};
}

inline String string_from_parts(const char* data, isize size) {
    return String{data, size};
}

inline String string_from_slice(Slice<u8> slice) {
    return String{(const char*)slice.data, slice.size};
}

inline const char* string_to_cstr_alloc(const String str, Allocator alloc) {
    char* data = core_alloc<char>(alloc, str.size + 1);
    memcpy(data, str.data, str.size);
    data[str.size] = '\0';

    return data;
}

inline const char* string_to_cstr(const String str, char* buffer,
                                  isize buffer_size) {
    core_assert_msg(buffer != nullptr, "Buffer is null");
    core_assert_msg(buffer_size > 0, "Buffer size is 0");
    core_assert_msg(str.data != nullptr, "String data is null");
    core_assert_msg(str.size >= 0, "String size is negative");
    core_assert_msg(buffer_size > str.size, "%ld > %ld", buffer_size, str.size);
    memcpy(buffer, str.data, str.size);
    buffer[str.size] = '\0';

    return buffer;
}

inline String string_substr(String str, isize start, isize count) {
    core_assert_msg(start >= 0, "%ld < 0", start);
    core_assert_msg(start + count <= str.size, "%ld + %ld > %ld", start, count,
                    str.size);

    return String{str.data + start, count};
}

inline isize string_utf8_size(String str) {
    char* s = (char*)str.data;
    size_t count = 0;
    while (*s) {
        count += (*s++ & 0xC0) != 0x80;
    }
    return count;
}

namespace std {
template <> struct std::hash<String> {
    std::size_t operator()(String str) const {
        std::size_t hash = 0;
        for (isize i = 0; i < str.size; i++) {
            hash = 31 * hash + str.data[i];
        }

        return hash;
    }
};
} // namespace std

struct RuneIterator {
    String str;
    isize index;
};

inline RuneIterator string_to_runes(String str) {
    return RuneIterator{str, 0};
}

inline rune rune_iter_next(RuneIterator* it) {
    core_assert(it != nullptr);
    core_assert(it->index >= 0);
    core_assert(it->index < it->str.size);

    RuneDetails codepoint = cstr_read_utf8_codepoint(it->str.data + it->index);
    it->index += codepoint.size;
    return codepoint.value;
}

inline bool rune_iter_done(RuneIterator* it) {
    core_assert(it != nullptr);
    return it->index >= it->str.size;
}

/// ------------------
/// Array
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
    array_init(&list, alloc, capacity);
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

template <typename T> inline void array_push_if_new(Array<T>* array, T item) {
    core_assert(array != nullptr);
    core_assert(array->alloc.alloc != nullptr);
    core_assert(array->items.data != nullptr);
    core_assert(array->items.size >= 0);
    core_assert(array->items.size <= array->capacity);

    if (!array_contains(array, item)) {
        array_push(array, item);
    }
}

template <typename T>
inline void array_push_slice(Array<T>* array, Slice<T> slice) {
    core_assert(array != nullptr);
    core_assert(array->alloc.alloc != nullptr);
    core_assert(array->items.data != nullptr);
    core_assert(array->items.size >= 0);
    core_assert(array->items.size <= array->capacity);

    isize new_size = array->items.size + slice.size;
    if (new_size > array->capacity) {
        isize new_capacity = std::max(array->capacity * 2, new_size);
        array->items.data = core_realloc<T>(array->alloc, array->items.data,
                                            array->capacity * sizeof(T),
                                            new_capacity * sizeof(T));
        array->capacity = new_capacity;
    }

    memcpy(array->items.data + array->items.size, slice.data,
           sizeof(T) * slice.size);
    array->items.size += slice.size;
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
    array->items[index] = array->items[array->items.size - 1];
    array->items.size -= 1;

    return value;
}

template <typename T>
inline bool array_remove_unordered(Array<T>* array, T value) {
    core_assert(array != nullptr);
    core_assert(array->alloc.alloc != nullptr);
    core_assert(array->items.data != nullptr);
    core_assert(array->items.size > 0);
    core_assert(array->items.size <= array->capacity);

    for (isize i = 0; i < array->items.size; i++) {
        if (array->items[i] == value) {
            array_remove_at_unordered(array, i);
            return true;
        }
    }
    return false;
}

template <typename T> inline isize array_index_of(Array<T>* array, T value) {
    core_assert(array != nullptr);
    core_assert(array->alloc.alloc != nullptr);
    core_assert(array->items.data != nullptr);
    core_assert(array->items.size > 0);
    core_assert(array->items.size <= array->capacity);

    for (isize i = 0; i < array->items.size; i++) {
        if (array->items[i] == value) {
            return i;
        }
    }
    return -1;
}

template <typename T> inline T array_remove_at(Array<T>* array, isize index) {
    core_assert_msg(index >= 0, "%ld < 0", index);
    core_assert_msg(index < array->items.size, "%ld >= %ld", index,
                    array->items.size);

    T value = array->items[index];
    memmove(array->items.data + index, array->items.data + index + 1,
            (array->items.size - index - 1) * sizeof(T));

    return value;
}

template <typename T> inline void array_remove(Array<T>* array, T value) {
    core_assert(array != nullptr);
    core_assert(array->alloc.alloc != nullptr);
    core_assert(array->items.data != nullptr);
    core_assert(array->items.size > 0);
    core_assert(array->items.size <= array->capacity);

    for (isize i = 0; i < array->items.size; i++) {
        if (array->items[i] == value) {
            array_remove_at(array, i);
            return;
        }
    }
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

template <typename T>
inline void array_swap(Array<T>* array, isize index_a, isize index_b) {
    core_assert_msg(index_a >= 0, "%ld < 0", index_a);
    core_assert_msg(index_b >= 0, "%ld < 0", index_b);
    core_assert_msg(index_a < array->size, "%ld >= %ld", index_a, array->size);
    core_assert_msg(index_b < array->size, "%ld >= %ld", index_b, array->size);

    T temp = array->items[index_a];
    array->items[index_a] = array->items[index_b];
    array->items[index_b] = temp;
}

template <typename T> inline bool array_contains(Array<T>* array, T value) {
    core_assert(array != nullptr);
    core_assert(array->alloc.alloc != nullptr);
    core_assert(array->items.data != nullptr);
    core_assert(array->items.size > 0);
    core_assert(array->items.size <= array->capacity);

    for (isize i = 0; i < array->items.size; i++) {
        if (array->items[i] == value) {
            return true;
        }
    }
    return false;
}

template <typename T> inline T array_last(Array<T>* array) {
    core_assert(array != nullptr);
    core_assert(array->alloc.alloc != nullptr);
    core_assert(array->items.data != nullptr);
    core_assert(array->items.size > 0);
    core_assert(array->items.size <= array->capacity);

    return array->items[array->items.size - 1];
}

/// ------------------
/// Ring buffer
/// ------------------

template <typename T> struct RingBuffer {
    Allocator alloc;
    T* data;
    isize capacity;
    isize size;
    // Head is the first element
    isize head;
    // Tail is one past the last element
    isize tail;

    T& operator[](isize index) {
        core_assert(index >= 0);
        core_assert(index < this->size);

        isize real_index = (head + index) % capacity;
        return this->data[real_index];
    }

    const T& operator[](isize index) const {
        core_assert(index >= 0);
        core_assert(index < this->size);

        isize real_index = (head + index) % capacity;
        return this->data[real_index];
    }
};

template <typename T>
inline void ring_buffer_init(RingBuffer<T>* ring_buffer, isize capacity,
                             Allocator alloc) {
    ring_buffer->alloc = alloc;
    ring_buffer->data = core_alloc<T>(alloc, capacity);
    ring_buffer->capacity = capacity;
    ring_buffer->size = 0;
    ring_buffer->head = 0;
    ring_buffer->tail = 0;
}

template <typename T>
inline RingBuffer<T> ring_buffer_make(isize capacity, Allocator alloc) {
    RingBuffer<T> buffer = {};
    ring_buffer_init(&buffer, capacity, alloc);
    return buffer;
}

template <typename T>
inline void ring_buffer_push_end(RingBuffer<T>* ring_buffer, T value) {
    core_assert(ring_buffer->tail <= ring_buffer->capacity);
    core_assert(ring_buffer->tail >= 0);
    core_assert(ring_buffer->head < ring_buffer->capacity);
    core_assert(ring_buffer->head >= 0);
    core_assert(ring_buffer->capacity > 0);
    core_assert(ring_buffer->data);

    // Grow
    if (ring_buffer->size == ring_buffer->capacity) {
        isize new_capacity = ring_buffer->capacity * 2;
        T* new_data = core_alloc<T>(ring_buffer->alloc, new_capacity);

        isize head = ring_buffer->head;
        isize tail = ring_buffer->tail;

        if (head < tail) {
            memcpy(new_data, ring_buffer->data + head,
                   sizeof(T) * (tail - head));
        } else {
            isize first_size = ring_buffer->capacity - head;
            memcpy(new_data, ring_buffer->data + head, sizeof(T) * first_size);
            memcpy(new_data + first_size, ring_buffer->data, sizeof(T) * tail);
        }

        ring_buffer->data = new_data;
        ring_buffer->capacity = new_capacity;
        ring_buffer->head = 0;
        ring_buffer->tail = ring_buffer->size;
    }

    ring_buffer->size += 1;
    if (ring_buffer->tail == ring_buffer->capacity) {
        ring_buffer->tail = 1;
        ring_buffer->data[0] = value;
        return;
    }

    ring_buffer->data[ring_buffer->tail] = value;
    ring_buffer->tail += 1;
}

template <typename T>
inline void ring_buffer_push_front(RingBuffer<T>* ring_buffer, T value) {
    core_assert(ring_buffer->tail <= ring_buffer->capacity);
    core_assert(ring_buffer->tail >= 0);
    core_assert(ring_buffer->head < ring_buffer->capacity);
    core_assert(ring_buffer->head >= 0);
    core_assert(ring_buffer->capacity > 0);
    core_assert(ring_buffer->data);

    // Grow
    if (ring_buffer->size == ring_buffer->capacity) {
        isize new_capacity = ring_buffer->capacity * 2;
        T* new_data = core_alloc<T>(ring_buffer->alloc, new_capacity);

        isize head = ring_buffer->head;
        isize tail = ring_buffer->tail;

        if (head < tail) {
            memcpy(new_data + 1, ring_buffer->data + head,
                   sizeof(T) * (tail - head));
        } else {
            isize first_size = ring_buffer->capacity - head;
            memcpy(new_data + 1, ring_buffer->data + head,
                   sizeof(T) * first_size);
            memcpy(new_data + first_size + 1, ring_buffer->data,
                   sizeof(T) * tail);
        }

        ring_buffer->data = new_data;
        ring_buffer->capacity = new_capacity;
        ring_buffer->head = 1;
        ring_buffer->tail = ring_buffer->size + 1;
    }

    if (ring_buffer->head == 0) {
        ring_buffer->head = ring_buffer->capacity - 1;
    } else {
        ring_buffer->head -= 1;
    }

    ring_buffer->size += 1;
    ring_buffer->data[ring_buffer->head] = value;
}

template <typename T>
inline T ring_buffer_pop_front(RingBuffer<T>* ring_buffer) {
    core_assert(ring_buffer->tail <= ring_buffer->capacity);
    core_assert(ring_buffer->tail >= 0);
    core_assert(ring_buffer->head < ring_buffer->capacity);
    core_assert(ring_buffer->head >= 0);
    core_assert(ring_buffer->capacity > 0);
    core_assert(ring_buffer->data);

    core_assert_msg(ring_buffer->size > 0, "ring_buffer is empty");

    T value = ring_buffer->data[ring_buffer->head];

    ring_buffer->head = (ring_buffer->head + 1) % ring_buffer->capacity;
    ring_buffer->size -= 1;

    return value;
}

template <typename T> inline T ring_buffer_pop_end(RingBuffer<T>* ring_buffer) {
    core_assert(ring_buffer->tail <= ring_buffer->capacity);
    core_assert(ring_buffer->tail >= 0);
    core_assert(ring_buffer->head < ring_buffer->capacity);
    core_assert(ring_buffer->head >= 0);
    core_assert(ring_buffer->capacity > 0);
    core_assert(ring_buffer->data);

    core_assert_msg(ring_buffer->size > 0, "ring_buffer is empty");

    ring_buffer->tail =
        (ring_buffer->tail - 1 + ring_buffer->capacity) % ring_buffer->capacity;
    T value = ring_buffer->data[ring_buffer->tail];

    ring_buffer->size -= 1;

    return value;
}

template <typename T>
inline bool ring_buffer_contains(RingBuffer<T>* ring_buffer, T value) {
    for (isize i = 0; i < ring_buffer->size; i++) {
        if ((*ring_buffer)[i] == value) {
            return true;
        }
    }

    return false;
}

/// ------------------
/// StaticArray
/// ------------------

#define ARRAY_PROGRAMMING_OVERLOADS(LEN)                                       \
    T& operator[](isize index) {                                               \
        core_assert(index >= 0);                                               \
        core_assert(index < LEN);                                              \
        return data[index];                                                    \
    }                                                                          \
                                                                               \
    const T& operator[](isize index) const {                                   \
        core_assert(index >= 0);                                               \
        core_assert(index < LEN);                                              \
        return data[index];                                                    \
    }                                                                          \
                                                                               \
    bool operator==(const StaticArray<T, LEN>& other) const {                  \
        for (isize i = 0; i < LEN; i++) {                                      \
            if (data[i] != other[i]) {                                         \
                return false;                                                  \
            }                                                                  \
        }                                                                      \
        return true;                                                           \
    }                                                                          \
                                                                               \
    bool operator!=(const StaticArray<T, LEN>& other) const {                  \
        return !(*this == other);                                              \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN> operator*(const StaticArray<T, LEN>& other) const {    \
        StaticArray<T, LEN> result;                                            \
        for (isize i = 0; i < LEN; i++) {                                      \
            result[i] = data[i] * other[i];                                    \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN> operator/(const StaticArray<T, LEN>& other) const {    \
        StaticArray<T, LEN> result;                                            \
        for (isize i = 0; i < LEN; i++) {                                      \
            result[i] = data[i] / other[i];                                    \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN> operator+(const StaticArray<T, LEN>& other) const {    \
        StaticArray<T, LEN> result;                                            \
        for (isize i = 0; i < LEN; i++) {                                      \
            result[i] = data[i] + other[i];                                    \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN> operator-(const StaticArray<T, LEN>& other) const {    \
        StaticArray<T, LEN> result;                                            \
        for (isize i = 0; i < LEN; i++) {                                      \
            result[i] = data[i] - other[i];                                    \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN>& operator*=(const StaticArray<T, LEN>& other) {        \
        for (isize i = 0; i < LEN; i++) {                                      \
            data[i] *= other[i];                                               \
        }                                                                      \
        return *this;                                                          \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN>& operator/=(const StaticArray<T, LEN>& other) {        \
        for (isize i = 0; i < LEN; i++) {                                      \
            data[i] /= other[i];                                               \
        }                                                                      \
        return *this;                                                          \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN>& operator+=(const StaticArray<T, LEN>& other) {        \
        for (isize i = 0; i < LEN; i++) {                                      \
            data[i] += other[i];                                               \
        }                                                                      \
        return *this;                                                          \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN>& operator-=(const StaticArray<T, LEN>& other) {        \
        for (isize i = 0; i < LEN; i++) {                                      \
            data[i] -= other[i];                                               \
        }                                                                      \
        return *this;                                                          \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN> operator*(T scalar) const {                            \
        StaticArray<T, LEN> result;                                            \
        for (isize i = 0; i < LEN; i++) {                                      \
            result[i] = data[i] * scalar;                                      \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN> operator/(T scalar) const {                            \
        StaticArray<T, LEN> result;                                            \
        for (isize i = 0; i < LEN; i++) {                                      \
            result[i] = data[i] / scalar;                                      \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN> operator+(T scalar) const {                            \
        StaticArray<T, LEN> result;                                            \
        for (isize i = 0; i < LEN; i++) {                                      \
            result[i] = data[i] + scalar;                                      \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN> operator-(T scalar) const {                            \
        StaticArray<T, LEN> result;                                            \
        for (isize i = 0; i < LEN; i++) {                                      \
            result[i] = data[i] - scalar;                                      \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN>& operator*=(T scalar) {                                \
        for (isize i = 0; i < LEN; i++) {                                      \
            data[i] *= scalar;                                                 \
        }                                                                      \
        return *this;                                                          \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN>& operator/=(T scalar) {                                \
        for (isize i = 0; i < LEN; i++) {                                      \
            data[i] /= scalar;                                                 \
        }                                                                      \
        return *this;                                                          \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN>& operator+=(T scalar) {                                \
        for (isize i = 0; i < LEN; i++) {                                      \
            data[i] += scalar;                                                 \
        }                                                                      \
        return *this;                                                          \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN>& operator-=(T scalar) {                                \
        for (isize i = 0; i < LEN; i++) {                                      \
            data[i] -= scalar;                                                 \
        }                                                                      \
        return *this;                                                          \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN> operator-() const {                                    \
        StaticArray<T, LEN> result;                                            \
        for (isize i = 0; i < LEN; i++) {                                      \
            result[i] = -data[i];                                              \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    StaticArray<T, LEN> operator+() const {                                    \
        return *this;                                                          \
    }

template <typename T, isize N> struct StaticArray {
    T data[N];
    static const isize size = N;

    ARRAY_PROGRAMMING_OVERLOADS(N)
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
template <typename T> struct StaticArray<T, 3> {
    union {
        T data[3];
        struct {
            T x, y, z;
        };
        struct {
            T r, g, b;
        };
    };
    static const isize size = 3;

    StaticArray(T x, T y, T z) : x(x), y(y), z(z) {
    }

    StaticArray() : data{0, 0, 0} {
    }

    ARRAY_PROGRAMMING_OVERLOADS(3)
};
static_assert(sizeof(StaticArray<f32, 3>) == sizeof(f32) * 3,
              "Vec3 size is incorrect");

template <typename T> struct StaticArray<T, 4> {
    union {
        T data[4];
        struct {
            T x, y, z, w;
        };
        struct {
            T r, g, b, a;
        };
    };
    static const isize size = 4;

    StaticArray(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {
    }

    StaticArray() : data{0, 0, 0, 0} {
    }

    ARRAY_PROGRAMMING_OVERLOADS(4)
};
static_assert(sizeof(StaticArray<f32, 4>) == sizeof(f32) * 4,
              "Vec3 size is incorrect");

template <typename T> struct StaticArray<T, 2> {
    union {
        T data[2];
        struct {
            T x, y;
        };
    };
    static const isize size = 2;

    StaticArray(T x, T y) : x(x), y(y) {
    }

    StaticArray() : data{0, 0} {
    }

    ARRAY_PROGRAMMING_OVERLOADS(2)
};

#pragma clang diagnostic pop

template <typename T, isize N>
inline Slice<T> static_array_to_slice(const StaticArray<T, N>& array) {
    return Slice<T>{array.data, N};
}

/// ------------------
/// BitSet
/// ------------------

struct BitSet {
    u8* data;
    isize size;
};

inline void bit_set_init(BitSet* bit_set, isize size, Allocator alloc) {
    core_assert_msg(size >= 0, "%ld < 0", size);
    isize byte_size = (size + 7) / 8;
    // Here we set the alignment to sizeof(u64), as in many of the functions we
    // do an optimization, where we treat the data as u64s.
    // On ARM and ARM64, this is important, as the compiler will not be able to
    // do this optimization automatically, as the alignment is not guaranteed to
    // be u64 which is a requirement on ARM.
    bit_set->data = core_alloc<u8>(alloc, byte_size, sizeof(u64));
    bit_set->size = size;
}

inline BitSet bit_set_make(isize size, Allocator alloc) {
    BitSet bit_set = {};
    bit_set_init(&bit_set, size, alloc);
    return bit_set;
}

inline BitSet bit_set_clone(const BitSet* bit_set, Allocator alloc) {
    BitSet new_bit_set = {};
    new_bit_set.size = bit_set->size;
    isize byte_size = (bit_set->size + 7) / 8;
    new_bit_set.data = core_alloc<u8>(alloc, byte_size);
    memcpy(new_bit_set.data, bit_set->data, byte_size);

    return new_bit_set;
}

inline void bit_set_set(BitSet* bit_set, isize index) {
    core_assert_msg(index >= 0, "%ld < 0", index);
    core_assert_msg(index < bit_set->size, "%ld >= %ld", index, bit_set->size);

    isize byte_index = index / 8;
    isize bit_index = index % 8;

    bit_set->data[byte_index] |= (1 << bit_index);
}

inline void bit_set_clear(BitSet* bit_set, isize index) {
    core_assert_msg(index >= 0, "%ld < 0", index);
    core_assert_msg(index < bit_set->size, "%ld >= %ld", index, bit_set->size);

    isize byte_index = index / 8;
    isize bit_index = index % 8;

    bit_set->data[byte_index] &= ~(1 << bit_index);
}

inline bool bit_set_get(const BitSet* bit_set, isize index) {
    core_assert_msg(index >= 0, "%ld < 0", index);
    core_assert_msg(index < bit_set->size, "%ld >= %ld", index, bit_set->size);

    isize byte_index = index / 8;
    isize bit_index = index % 8;

    return (bit_set->data[byte_index] & (1 << bit_index)) != 0;
}

#define bit_set_combine_loop(op)                                               \
    core_assert_msg(a->size == b->size, "%ld != %ld", a->size, b->size);       \
                                                                               \
    isize bytes = (a->size + 7) / 8;                                           \
    isize i = 0;                                                               \
                                                                               \
    while (bytes - i >= 8) {                                                   \
        u64* a_data = &((u64*)(a->data))[i];                                   \
        u64 b_data = ((u64*)(b->data))[i];                                     \
        *a_data = *a_data op b_data;                                           \
        i += 8;                                                                \
    }                                                                          \
                                                                               \
    while (bytes - i >= 4) {                                                   \
        u32* a_data = &((u32*)(a->data))[i];                                   \
        u32 b_data = ((u32*)(b->data))[i];                                     \
        *a_data = *a_data op b_data;                                           \
        i += 4;                                                                \
    }                                                                          \
                                                                               \
    while (bytes - i >= 1) {                                                   \
        a->data[i] = a->data[i] op b->data[i];                                 \
        i += 1;                                                                \
    }

inline void bit_set_and(BitSet* a, const BitSet* b) {
    bit_set_combine_loop(&)
}

inline void bit_set_or(BitSet* a, const BitSet* b) {
    bit_set_combine_loop(|)
}

inline void bit_set_xor(BitSet* a, const BitSet* b) {
    bit_set_combine_loop(^)
}

inline void bit_set_not(BitSet* a) {
    isize bytes = (a->size + 7) / 8;
    for (isize i = 0; i < bytes; i++) {
        a->data[i] = ~a->data[i];
    }

    // Make sure to clear the last bits
    isize last_bits = a->size % 8;
    if (last_bits > 0) {
        u8 mask = (1 << last_bits) - 1;
        a->data[bytes - 1] &= ~mask;
    }
}

inline isize bit_set_count(const BitSet* a) {
    isize count = 0;
    isize bytes = (a->size + 7) / 8;
    isize i = 0;

    while (bytes - i >= 8) {
        u64 value = ((u64*)(a->data))[i];
        count += __builtin_popcountll(value);
        i += 8;
    }

    if (bytes - i > 0) {
        u64 value = 0;
        u8* value_ptr = (u8*)&value;
        core_assert(bytes - i < 8);
        memcpy(value_ptr, a->data + i, bytes - i);
        count += __builtin_popcountll(value);
    }

    return count;
}

inline bool bit_set_equals(const BitSet* a, const BitSet* b) {
    core_assert_msg(a->size == b->size, "%ld != %ld", a->size, b->size);
    isize bytes = (a->size + 7) / 8;
    return memcmp(a->data, b->data, bytes) == 0;
}

inline usize bit_set_hash(const BitSet* a) {
    usize hash = 0xcbf29ce484222325;
    isize bytes = (a->size + 7) / 8;

    for (isize i = 0; i < bytes; i++) {
        hash ^= a->data[i];
        hash *= 0x100000001b3;
    }

    return hash;
}

inline bool bit_set_is_empty(const BitSet* a) {
    isize bytes = (a->size + 7) / 8;
    for (isize i = 0; i < bytes; i++) {
        if (a->data[i] != 0) {
            return false;
        }
    }

    return true;
}

/// ------------------
/// STL compat allocator
/// ------------------

template <typename T> struct StlCompatAllocator {
    using value_type = T;

    Allocator alloc;

    StlCompatAllocator(Allocator alloc) : alloc(alloc) {
    }

    template <typename U>
    StlCompatAllocator(const StlCompatAllocator<U>& other)
        : alloc(other.alloc) {
    }

    T* allocate(std::size_t n) {
        return core_alloc<T>(alloc, n);
    }

    void deallocate(T* pointer, std::size_t) {
        core_free(alloc, pointer);
    }

    template <typename U>
    bool operator==(const StlCompatAllocator<U>& other) const {
        return alloc == other.alloc;
    }

    template <typename U>
    bool operator!=(const StlCompatAllocator<U>& other) const {
        return !(*this == other);
    }
};

/// ------------------
/// Hash map
/// ------------------

template <typename K, typename V> struct HashMap {
    Allocator alloc;
    std::unordered_map<K, V, std::hash<K>, std::equal_to<K>,
                       StlCompatAllocator<std::pair<const K, V>>>* backing_map;
};

template <typename K, typename V>
inline void hash_map_init(HashMap<K, V>* hash_map, isize default_size,
                          Allocator alloc) {
    hash_map->alloc = alloc;

    StlCompatAllocator<K> allocator(alloc);
    hash_map->backing_map =
        new (core_alloc<
             std::unordered_map<K, V, std::hash<K>, std::equal_to<K>,
                                StlCompatAllocator<std::pair<const K, V>>>>(
            alloc))
            std::unordered_map<K, V, std::hash<K>, std::equal_to<K>,
                               StlCompatAllocator<std::pair<const K, V>>>(
                default_size, std::hash<K>(), std::equal_to<K>(), allocator);

    core_assert(hash_map->backing_map);
}

template <typename K, typename V>
inline void hash_map_insert_or_set(HashMap<K, V>* hash_map, K key, V value) {
    (*hash_map->backing_map)[key] = value;
}

template <typename K, typename V>
inline V hash_map_must_get(HashMap<K, V>* hash_map, K key) {
    auto it = hash_map->backing_map->find(key);
    core_assert_msg(it != hash_map->backing_map->end(), "Key not found");
    return it->second;
}

template <typename K, typename V>
inline V* hash_map_get_ptr(HashMap<K, V>* hash_map, K key) {
    auto it = hash_map->backing_map->find(key);
    if (it == hash_map->backing_map->end()) {
        return nullptr;
    }

    return &it->second;
}

template <typename K, typename V>
inline void hash_map_remove(HashMap<K, V>* hash_map, K key) {
    hash_map->backing_map->erase(key);
}

/// ----------------
/// HashSet
/// ----------------

template <typename T> struct HashSet {
    Allocator alloc;
    std::unordered_set<T, std::hash<T>, std::equal_to<T>,
                       StlCompatAllocator<T>>* backing_set;
};

template <typename T>
inline void hash_set_init(HashSet<T>* hash_set, isize default_size,
                          Allocator alloc) {
    hash_set->alloc = alloc;

    StlCompatAllocator<T> allocator(alloc);
    hash_set->backing_set =
        new (core_alloc<std::unordered_set<T, std::hash<T>, std::equal_to<T>,
                                           StlCompatAllocator<T>>>(alloc))
            std::unordered_set<T, std::hash<T>, std::equal_to<T>,
                               StlCompatAllocator<T>>(
                default_size, std::hash<T>(), std::equal_to<T>(), allocator);

    core_assert(hash_set->backing_set);
}

template <typename T>
inline HashSet<T> hash_set_make(isize default_size, Arena* arena) {
    HashSet<T> hash_set = {};
    hash_set_init(&hash_set, default_size, arena);
    return hash_set;
}

template <typename T>
inline bool hash_set_insert(HashSet<T>* hash_set, T value) {
    auto result = hash_set->backing_set->insert(value);
    return result.second;
}

template <typename T>
inline bool hash_set_contains(HashSet<T>* hash_set, T value) {
    auto it = hash_set->backing_set->find(value);
    return it != hash_set->backing_set->end();
}

template <typename T>
inline const T* hash_set_get_ptr(HashSet<T>* hash_set, T value) {
    auto it = hash_set->backing_set->find(value);
    if (it == hash_set->backing_set->end()) {
        return nullptr;
    }
    return &(*it);
}

template <typename T>
inline void hash_set_remove(HashSet<T>* hash_set, T value) {
    hash_set->backing_set->erase(value);
}

/// ----------------
/// Files
/// ----------------

const isize MAX_FILE_SIZE = 10 * 1024 * 1024 * 1024l; // 1 GB

enum class FileReadError {
    FileNotFound,
    PermissionsDenied,
    SystemError,
    ReadError,
    InvalidFile,
    SizeTooLarge
};

inline Result<Slice<u8>, FileReadError>
file_read_full(const String path, Allocator alloc,
               isize max_allowed_size = MAX_FILE_SIZE) {

    char path_buffer[PATH_MAX];
    string_to_cstr(path, path_buffer, sizeof(path_buffer));

    FILE* file = fopen(path_buffer, "rb");
    if (!file) {
        switch (errno) {
        case ENOENT:
            return result_err(FileReadError::FileNotFound);
        case EACCES:
        case EPERM:
            return result_err(FileReadError::PermissionsDenied);
        default:
            return result_err(FileReadError::SystemError);
        }
    }
    defer(fclose(file));

    if (fseek(file, 0, SEEK_END) != 0) {
        return result_err(FileReadError::InvalidFile);
    }

    long size = ftell(file);
    if (size < 0) {
        return result_err(FileReadError::InvalidFile);
    }

    if (size > max_allowed_size) {
        return result_err(FileReadError::SizeTooLarge);
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        return result_err(FileReadError::InvalidFile);
    }

    Slice<u8> data;
    data = slice_make<u8>(size, alloc);

    isize bytes_read = 0;
    while (bytes_read < size) {
        isize read = fread(data.data + bytes_read, 1, size - bytes_read, file);
        if (read <= 0) {
            if (ferror(file)) {
                return result_err(FileReadError::ReadError);
            }
            break;
        }
        bytes_read += read;
    }

    if (bytes_read != size) {
        return result_err(FileReadError::ReadError);
    }

    return result_ok(data);
}
