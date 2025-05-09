#include "core.hpp"
#include <gtest/gtest.h>

TEST(Core, Slice) {
    int values[] = {1, 2, 3, 4, 5};
    Slice<int> slice = slice_from_parts(values, 5);

    for (int value : slice) {
        EXPECT_TRUE(value >= 1 && value <= 5);
    }
}

TEST(Core, ArenaAlloc) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    int* data = core_alloc<int>(alloc, 10);

    EXPECT_NE(data, nullptr);
    EXPECT_EQ(arena.offset, 10 * sizeof(int));
    EXPECT_EQ(arena.data.size, 1024);
    EXPECT_NE(arena.data.data, nullptr);
    EXPECT_EQ(arena.data.data, buff.data);
}

TEST(Core, ArenaReallocSuccess) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    int* data = core_alloc<int>(alloc, 10);

    EXPECT_NE(data, nullptr);
    EXPECT_EQ(arena.offset, 10 * sizeof(int));
    EXPECT_EQ(arena.data.size, 1024);
    EXPECT_NE(arena.data.data, nullptr);
    EXPECT_EQ(arena.data.data, buff.data);

    int* new_data =
        core_realloc<int>(alloc, data, 10 * sizeof(int), 20 * sizeof(int));

    EXPECT_NE(new_data, nullptr);
    EXPECT_EQ(arena.offset, 20 * sizeof(int));
    EXPECT_EQ(arena.data.size, 1024);
    EXPECT_NE(arena.data.data, nullptr);
    EXPECT_EQ(arena.data.data, buff.data);
    EXPECT_EQ(new_data, data);
}

TEST(Core, ArenaReallocFail) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    int* data = core_alloc<int>(alloc, 10);

    EXPECT_NE(data, nullptr);
    EXPECT_EQ(arena.offset, 10 * sizeof(int));
    EXPECT_EQ(arena.data.size, 1024);
    EXPECT_NE(arena.data.data, nullptr);
    EXPECT_EQ(arena.data.data, buff.data);

    int* middle = core_alloc<int>(alloc);
    EXPECT_NE(middle, nullptr);

    int* new_data =
        core_realloc<int>(alloc, data, 10 * sizeof(int), 20 * sizeof(int));

    EXPECT_NE(new_data, nullptr);
    EXPECT_EQ(arena.offset, (10 + 1 + 20) * sizeof(int));
    EXPECT_EQ(arena.data.size, 1024);
    EXPECT_NE(arena.data.data, nullptr);
    EXPECT_EQ(arena.data.data, buff.data);
    EXPECT_NE(new_data, data);
}

TEST(Core, DynamicArenaAlloc) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena backing_arena = arena_make(buff);

    DynamicArena dynamic_arena =
        dynamic_arena_make(64, arena_allocator(&backing_arena));
    Allocator alloc = dynamic_arena_allocator(&dynamic_arena);

    u8* data = core_alloc<u8>(alloc, 32);
    EXPECT_NE(data, nullptr);
    EXPECT_EQ(dynamic_arena.current->size, 32);
    EXPECT_EQ(dynamic_arena.current->capacity, 64);
    EXPECT_EQ(dynamic_arena.current->prev, nullptr);

    u8* data2 = core_alloc<u8>(alloc, 48);
    EXPECT_NE(data2, nullptr);
    EXPECT_EQ(dynamic_arena.current->size, 48);
    EXPECT_EQ(dynamic_arena.current->capacity, 64);
    EXPECT_NE(dynamic_arena.current->prev, nullptr);

    u8* data3 = core_alloc<u8>(alloc, 128);
    EXPECT_NE(data3, nullptr);
    EXPECT_EQ(dynamic_arena.current->size, 128);
    EXPECT_EQ(dynamic_arena.current->capacity, 128);
    EXPECT_NE(dynamic_arena.current->prev, nullptr);
    EXPECT_EQ(dynamic_arena.current->prev->size, 48);
    EXPECT_EQ(dynamic_arena.current->prev->capacity, 64);
}
