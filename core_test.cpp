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

TEST(Core, ArrayProgrammingVec3) {
    using Vector3 = StaticArray<float, 3>;

    Vector3 vec = {1.0f, 2.0f, 3.0f};
    Vector3 vec2 = {4.0f, 5.0f, 6.0f};

    Vector3 vec3 = vec + vec2;

    Vector3 vec4 = vec3 * 2.0f;

    EXPECT_EQ(vec4.x, 10.0f);
    EXPECT_EQ(vec4.y, 14.0f);
    EXPECT_EQ(vec4.z, 18.0f);
}

TEST(Core, ArrayProgrammingVec4) {
    using Vector4 = StaticArray<float, 4>;

    Vector4 vec = {1.0f, 2.0f, 3.0f, 0.0f};
    Vector4 vec2 = {4.0f, 5.0f, 6.0f, 10.0f};

    Vector4 vec3 = vec + vec2;

    Vector4 vec4 = vec3 * 2.0f;

    EXPECT_EQ(vec4.x, 10.0f);
    EXPECT_EQ(vec4.y, 14.0f);
    EXPECT_EQ(vec4.z, 18.0f);
    EXPECT_EQ(vec4.w, 20.0f);
}

TEST(Core, RingBuffer) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    RingBuffer<int> buffer = ring_buffer_make<int>(4, alloc);

    ring_buffer_push_end(&buffer, 1);
    ring_buffer_push_end(&buffer, 2);
    ring_buffer_push_end(&buffer, 3);

    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[1], 2);
    EXPECT_EQ(buffer[2], 3);
    EXPECT_EQ(buffer.size, 3);

    EXPECT_EQ(ring_buffer_pop_front(&buffer), 1);
    EXPECT_EQ(buffer.size, 2);

    ring_buffer_push_end(&buffer, 4);
    ring_buffer_push_end(&buffer, 5); // Should cause growth

    EXPECT_EQ(buffer[0], 2);
    EXPECT_EQ(buffer[1], 3);
    EXPECT_EQ(buffer[2], 4);
    EXPECT_EQ(buffer[3], 5);

    EXPECT_TRUE(ring_buffer_contains(&buffer, 4));
    EXPECT_FALSE(ring_buffer_contains(&buffer, 1));

    EXPECT_EQ(ring_buffer_pop_end(&buffer), 5);
    EXPECT_EQ(buffer.size, 3);
}

TEST(Core, RingBufferPushFront) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    RingBuffer<int> buffer = ring_buffer_make<int>(4, alloc);

    // Test basic push_front operations
    ring_buffer_push_front(&buffer, 3);
    ring_buffer_push_front(&buffer, 2);
    ring_buffer_push_front(&buffer, 1);

    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[1], 2);
    EXPECT_EQ(buffer[2], 3);
    EXPECT_EQ(buffer.size, 3);

    // Test push_front with buffer wrap-around
    ring_buffer_push_front(&buffer, 0);
    ring_buffer_push_front(&buffer, -1); // Should cause growth

    EXPECT_EQ(buffer[0], -1);
    EXPECT_EQ(buffer[1], 0);
    EXPECT_EQ(buffer[2], 1);
    EXPECT_EQ(buffer[3], 2);
    EXPECT_EQ(buffer[4], 3);
    EXPECT_EQ(buffer.size, 5);

    // Verify elements are in correct order after popping
    EXPECT_EQ(ring_buffer_pop_front(&buffer), -1);
    EXPECT_EQ(ring_buffer_pop_end(&buffer), 3);
    EXPECT_EQ(buffer.size, 3);
}

TEST(Core, BitSet) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    BitSet bits = bit_set_make(32, alloc);

    // Test set/get
    bit_set_set(&bits, 0);
    bit_set_set(&bits, 31);
    EXPECT_TRUE(bit_set_get(&bits, 0));
    EXPECT_TRUE(bit_set_get(&bits, 31));
    EXPECT_FALSE(bit_set_get(&bits, 15));

    // Test clear
    bit_set_clear(&bits, 0);
    EXPECT_FALSE(bit_set_get(&bits, 0));

    // Test count
    EXPECT_EQ(bit_set_count(&bits), 1);

    // Test logical operations
    BitSet bits2 = bit_set_make(32, alloc);
    bit_set_set(&bits2, 31);
    bit_set_set(&bits2, 30);

    BitSet bits3 = bit_set_clone(&bits, alloc);
    EXPECT_TRUE(bit_set_equals(&bits, &bits3));

    bit_set_and(&bits, &bits2);
    EXPECT_EQ(bit_set_count(&bits), 1);

    bit_set_or(&bits3, &bits2);
    EXPECT_EQ(bit_set_count(&bits3), 2);

    bit_set_xor(&bits3, &bits2);
    EXPECT_EQ(bit_set_count(&bits3), 0);

    EXPECT_TRUE(bit_set_is_empty(&bits3));
}

TEST(Core, HashMap) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    HashMap<String, int> map;
    hash_map_init(&map, 16, alloc);

    String key1 = string_from_cstr_alloc("test1", alloc);
    String key2 = string_from_cstr_alloc("test2", alloc);

    // Test insert and get
    hash_map_insert_or_set(&map, key1, 42);
    hash_map_insert_or_set(&map, key2, 84);

    EXPECT_EQ(hash_map_must_get(&map, key1), 42);
    EXPECT_EQ(hash_map_must_get(&map, key2), 84);

    // Test get pointer
    int* value_ptr = hash_map_get_ptr(&map, key1);
    EXPECT_NE(value_ptr, nullptr);
    EXPECT_EQ(*value_ptr, 42);

    // Test update
    hash_map_insert_or_set(&map, key1, 100);
    EXPECT_EQ(hash_map_must_get(&map, key1), 100);

    // Test remove
    hash_map_remove(&map, key1);
    EXPECT_EQ(hash_map_get_ptr(&map, key1), nullptr);
}

TEST(Core, HashSet) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    HashSet<int> set;
    hash_set_init(&set, 16, alloc);

    // Test insert and contains
    EXPECT_TRUE(hash_set_insert(&set, 42));
    EXPECT_TRUE(hash_set_contains(&set, 42));
    EXPECT_FALSE(hash_set_contains(&set, 84));

    // Test duplicate insert
    EXPECT_FALSE(hash_set_insert(&set, 42));

    // Test get pointer
    const int* value_ptr = hash_set_get_ptr(&set, 42);
    EXPECT_NE(value_ptr, nullptr);
    EXPECT_EQ(*value_ptr, 42);

    // Test remove
    hash_set_remove(&set, 42);
    EXPECT_FALSE(hash_set_contains(&set, 42));
    EXPECT_EQ(hash_set_get_ptr(&set, 42), nullptr);
}

TEST(Core, ArrayOperations) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    Array<int> arr = array_make<int>(alloc, 4);

    // Test push and access
    array_push(&arr, 1);
    array_push(&arr, 2);
    array_push(&arr, 3);

    EXPECT_EQ(arr.items[0], 1);
    EXPECT_EQ(arr.items[1], 2);
    EXPECT_EQ(arr.items[2], 3);

    // Test push_slice
    int values[] = {4, 5};
    array_push_slice(&arr, slice_from_parts(values, 2));
    EXPECT_EQ(arr.items.size, 5);

    // Test pop
    EXPECT_EQ(array_pop(&arr), 5);

    // Test clear
    array_clear(&arr);
    EXPECT_EQ(arr.items.size, 0);

    // Test remove operations
    array_push(&arr, 1);
    array_push(&arr, 2);
    array_push(&arr, 3);

    EXPECT_EQ(array_remove_at_unordered(&arr, 1), 2);
    EXPECT_TRUE(array_remove_unordered(&arr, 3));

    array_push(&arr, 4);
    array_push(&arr, 5);
    EXPECT_EQ(array_remove_at(&arr, 1), 4);

    // Test last
    EXPECT_EQ(array_last(&arr), 5);
}

TEST(Core, StringBasics) {
    String str = string_from_cstr("Hello");
    EXPECT_EQ(str.size, 5);
    EXPECT_EQ(str[0], 'H');
    EXPECT_EQ(str[4], 'o');

    String str2 = string_from_cstr("Hello");
    EXPECT_TRUE(str == str2);
    EXPECT_TRUE(str == "Hello");

    String substr = string_substr(str, 1, 2);
    EXPECT_EQ(substr.size, 2);
    EXPECT_EQ(substr[0], 'e');
    EXPECT_EQ(substr[1], 'l');
}

TEST(Core, StringAllocation) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    String str = string_from_cstr_alloc("Test String", alloc);
    EXPECT_EQ(str.size, 11);
    EXPECT_TRUE(str == "Test String");

    const char* cstr = string_to_cstr_alloc(str, alloc);
    EXPECT_STREQ(cstr, "Test String");
}

TEST(Core, Utf8Basic) {
    // Test basic ASCII
    RuneDetails details = cstr_read_utf8_codepoint("A");
    EXPECT_EQ(details.value.codepoint, 65); // ASCII value for 'A'
    EXPECT_EQ(details.size, 1);

    // Test 2-byte UTF-8 (é)
    details = cstr_read_utf8_codepoint("é");
    EXPECT_EQ(details.value.codepoint, 0x00E9);
    EXPECT_EQ(details.size, 2);

    // Test 3-byte UTF-8 (€)
    details = cstr_read_utf8_codepoint("€");
    EXPECT_EQ(details.value.codepoint, 0x20AC);
    EXPECT_EQ(details.size, 3);

    // Test 4-byte UTF-8 (🙂)
    details = cstr_read_utf8_codepoint("🙂");
    EXPECT_EQ(details.value.codepoint, 0x1F642);
    EXPECT_EQ(details.size, 4);
}

TEST(Core, RuneConversion) {
    char buffer[5];

    // Test ASCII
    rune r = rune_from_cstr("A");
    rune_to_cstr(r, buffer);
    EXPECT_STREQ(buffer, "A");

    // Test 2-byte UTF-8
    r = rune_from_cstr("é");
    rune_to_cstr(r, buffer);
    EXPECT_STREQ(buffer, "é");

    // Test rune comparison
    EXPECT_TRUE(r == "é");
    EXPECT_FALSE(r == "e");
}

TEST(Core, StringUtf8Size) {
    // Test string with mix of ASCII and UTF-8
    String str = string_from_cstr("Hello 世界");
    EXPECT_EQ(string_utf8_size(str), 8);

    String str2 = string_from_cstr("🙂👋");
    EXPECT_EQ(string_utf8_size(str2), 2);
}

TEST(Core, RuneIteration) {
    String str = string_from_cstr("Hello 世界");
    RuneIterator it = string_to_runes(str);

    // Test ASCII characters
    EXPECT_FALSE(rune_iter_done(&it));
    EXPECT_EQ(rune_iter_next(&it), 'H');
    EXPECT_EQ(rune_iter_next(&it), 'e');
    EXPECT_EQ(rune_iter_next(&it), 'l');
    EXPECT_EQ(rune_iter_next(&it), 'l');
    EXPECT_EQ(rune_iter_next(&it), 'o');
    EXPECT_EQ(rune_iter_next(&it), ' ');

    // Test Chinese characters
    EXPECT_EQ(rune_iter_next(&it).codepoint, 0x4E16); // 世
    EXPECT_EQ(rune_iter_next(&it).codepoint, 0x754C); // 界

    EXPECT_TRUE(rune_iter_done(&it));
}

TEST(Core, ReadFileFull) {
    Slice<u8> buff = slice_make<u8>(1024, c_allocator());
    defer(core_free(c_allocator(), buff.data));
    Arena arena = arena_make(buff);
    Allocator alloc = arena_allocator(&arena);

    Result<Slice<u8>, FileReadError> file_data =
        file_read_full(string_from_cstr("../testfile.txt"), alloc);

    EXPECT_TRUE(file_data.is_ok);
    core_assert(file_data.is_ok);
    String data = string_from_slice(file_data.value);
    EXPECT_EQ(file_data.value.size, 14);
    EXPECT_EQ(data, "Hello, World!\n");
}
