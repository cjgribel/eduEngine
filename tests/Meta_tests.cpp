#include <gtest/gtest.h>
#include "entt/entt.hpp"
#include "MetaLiterals.h"
#include "MetaInfo.h"
#include "MetaAux.h"

using namespace eeng;

struct MockType
{
    int x{ 2 };
    float y{ 3.0f };

    // TODO
    // bool flag = true;
    // debugvec3 position;
    // std::string somestring = "Hello";
    // // std::vector<int> vector1 = { 1, 2, 3 };
    // std::array<int, 3> vector1 = { 1, 2, 3 };
    // std::vector<ElementType> vector2 = { {4.0f}, {5.0f}, {6.0f} };
    // std::map<int, float> map1 = { {7, 7.5f}, {8, 8.5f} };
    // std::map<int, ElementType> map2 = { {9, {9.5f}}, {10, {10.5f}} };
    // std::map<ElementType, int> map3 = { {{9.5f}, 9}, {{10.5f}, 10} };
    // std::set<int> set1 = { 11, 12 };

    enum class AnEnum : int { Hello = 5, Bye = 6, Hola = 8 } an_enum{ AnEnum::Hello };

    // Function with value, reference, const reference, pointer, const pointer
    int mutate_and_sum(int a, float& b, const float& c, double* d, const double* e) const
    {
        b += 1.5f;        // mutate non-const ref
        *d += 2.5;        // mutate non-const pointer
        return a + static_cast<int>(b) + static_cast<int>(*d) + static_cast<int>(c) + static_cast<int>(*e);
    }
};

// Print policy of entt::any_policy
inline std::string policy_to_string(entt::any_policy policy)
{
    switch (policy) {
    case entt::any_policy::embedded: return "embedded";
    case entt::any_policy::ref: return "ref";
    case entt::any_policy::cref: return "cref";
    case entt::any_policy::dynamic: return "dynamic";
    default: return "unknown";
    }
}

// Test Fixture
class MetaRegistrationTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        // Register MockType::AnEnum
        auto enum_info = EnumMetaInfo{
            .display_name = "AnEnum",
            .tooltip = "AnEnum is a test enum with three values.",
            .underlying_type = entt::resolve<std::underlying_type_t<MockType::AnEnum>>()
        };
        entt::meta_factory<MockType::AnEnum>()
            .type("AnEnum"_hs)
            .custom<EnumMetaInfo>(enum_info)

            .data<MockType::AnEnum::Hello>("Hello"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Hello", "Greeting in English." })
            .traits(MetaFlags::none)

            .data<MockType::AnEnum::Bye>("Bye"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Bye", "Farewell in English." })
            .traits(MetaFlags::none)

            .data<MockType::AnEnum::Hola>("Hola"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Hola", "Greeting in Spanish." })
            .traits(MetaFlags::none);

        // Register MockType
        entt::meta_factory<MockType>{}
        .type("MockType"_hs)
            .custom<TypeMetaInfo>(TypeMetaInfo{ "MockType", "A mock resource type." })
            .traits(MetaFlags::none)

            .data<&MockType::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "Integer member x." })
            .traits(MetaFlags::read_only)

            .data<&MockType::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Float member y." })
            .traits(MetaFlags::read_only | MetaFlags::hidden)

            .func<&MockType::mutate_and_sum>("mutate_and_sum"_hs)
            .custom<FuncMetaInfo>(FuncMetaInfo{ "mutate_and_sum", "Mutates ref and ptr args, and sums them." })
            .traits(MetaFlags::none);
    }
};

TEST(MetaAnyPolicyTest, VerifyMetaAnyBasePolicies)
{
    int value = 42;
    const int const_value = 99;

    // 1. Small object — embedded
    {
        entt::meta_any meta_any{ value };

        EXPECT_TRUE(meta_any) << "meta_any is invalid (no type info)";
        EXPECT_TRUE(meta_any.base()) << "meta_any is empty (no value stored)";
        EXPECT_EQ(meta_any.base().policy(), entt::any_policy::embedded) << "Expected embedded policy for small int";
    }

    // 2. "Big" object — dynamic
    {
        struct BigObject { int c[8]; }; // 32 bytes

        BigObject big_object;
        entt::meta_any meta_any{ big_object };

        EXPECT_TRUE(meta_any) << "meta_any is invalid (no type info)";
        EXPECT_TRUE(meta_any.base()) << "meta_any is empty (no value stored)";
        EXPECT_EQ(meta_any.base().policy(), entt::any_policy::dynamic) << "Expected dynamic policy for big object";
    }

    // 3. Reference
    {
        /*
        Note: for this code,
            entt::meta_any ma_ref{ std::ref(value) };
        the meta_any will contain a std::reference_wrapper<int> by value (entt::any_policy::embedded).
        Instead, we use entt::forward_as_meta to create a meta_any that holds a reference (entt::any_policy::ref).
        */

        entt::meta_any meta_any = entt::forward_as_meta(value);

        EXPECT_TRUE(meta_any) << "meta_any is invalid (no type info)";
        EXPECT_TRUE(meta_any.base()) << "meta_any is empty (no value stored)";
        EXPECT_EQ(meta_any.base().policy(), entt::any_policy::ref);
    }

    // 4. Const Reference
    {
        entt::meta_any meta_any = entt::forward_as_meta(const_value);

        EXPECT_TRUE(meta_any) << "meta_any is invalid (no type info)";
        EXPECT_TRUE(meta_any.base()) << "meta_any is empty (no value stored)";
        EXPECT_EQ(meta_any.base().policy(), entt::any_policy::cref);
    }

    // 5. Pointer
    {
        int* ptr = &value;
        entt::meta_any meta_any{ ptr };

        EXPECT_TRUE(meta_any) << "meta_any is invalid (no type info)";
        EXPECT_TRUE(meta_any.base()) << "meta_any is empty (no value stored)";
        EXPECT_EQ(meta_any.base().policy(), entt::any_policy::embedded) << "Expected embedded policy for small int pointer";
    }

    // 6. Const Pointer
    {
        const int* cptr = &const_value;
        entt::meta_any meta_any{ cptr };

        EXPECT_TRUE(meta_any) << "meta_any is invalid (no type info)";
        EXPECT_TRUE(meta_any.base()) << "meta_any is empty (no value stored)";
        EXPECT_EQ(meta_any.base().policy(), entt::any_policy::embedded) << "Expected embedded policy for small const int pointer";
    }
}

// Actual test case
TEST_F(MetaRegistrationTest, VerifyMetaInformation)
{
    // Resolve the type
    auto meta_type = entt::resolve<MockType>();
    ASSERT_TRUE(meta_type);

    // Check custom data and traits for the type
    {
        TypeMetaInfo* type_info = meta_type.custom();
        ASSERT_NE(type_info, nullptr);
        EXPECT_EQ(type_info->display_name, "MockType");
        EXPECT_EQ(type_info->tooltip, "A mock resource type.");

        auto type_flags = meta_type.traits<MetaFlags>();
        EXPECT_FALSE(any(type_flags)); // should be MetaFlags::none
    }

    // Check custom data and traits for members
    for (auto [id_type, meta_data] : meta_type.data())
    {
        DataMetaInfo* member_info = meta_data.custom();
        ASSERT_NE(member_info, nullptr);

        auto flags = meta_data.traits<MetaFlags>();
        EXPECT_TRUE(any(flags)); // must have at least some flags set

        if (member_info->display_name == "x")
        {
            EXPECT_EQ(member_info->tooltip, "Integer member x.");
            EXPECT_TRUE((flags & MetaFlags::read_only) == MetaFlags::read_only);
            EXPECT_FALSE(any(flags & MetaFlags::hidden));
        }
        else if (member_info->display_name == "y")
        {
            EXPECT_EQ(member_info->tooltip, "Float member y.");
            EXPECT_TRUE((flags & MetaFlags::read_only) == MetaFlags::read_only);
            EXPECT_TRUE((flags & MetaFlags::hidden) == MetaFlags::hidden);
        }
        else
        {
            FAIL() << "Unexpected member: " << member_info->display_name;
        }
    }

    // Check custom data and traits for the "mutate_and_sum" function
    {
        // Directly fetch "mutate_and_sum" function
        auto func_meta = meta_type.func("mutate_and_sum"_hs);
        ASSERT_TRUE(func_meta);

        // Validate custom info
        FuncMetaInfo* func_info = func_meta.custom();
        ASSERT_NE(func_info, nullptr);
        EXPECT_EQ(func_info->display_name, "mutate_and_sum");
        EXPECT_EQ(func_info->tooltip, "Mutates ref and ptr args, and sums them.");

        // Validate traits
        auto flags = func_meta.traits<MetaFlags>();
        EXPECT_FALSE(any(flags)); // No flags set on the function
    }
}

TEST_F(MetaRegistrationTest, VerifyMutateAndSumFunctionCall)
{
    auto type_meta = entt::resolve<MockType>();
    ASSERT_TRUE(type_meta);

    // Fetch function
    auto func_meta = type_meta.func("mutate_and_sum"_hs);
    ASSERT_TRUE(func_meta) << "Failed to resolve 'mutate_and_sum'";

    // Prepare instance
    MockType instance{ 42, 3.14f };

    // Prepare arguments
    int arg_value = 10;                 // int by value
    float arg_ref = 2.5f;               // float by non-const reference
    const float arg_const_ref = 4.5f;         // float by const reference
    double arg_ptr_value = 7.5;         // double by non-const pointer
    double arg_const_ptr_value = 5.5;   // double by const pointer

    double* arg_ptr = &arg_ptr_value;
    const double* arg_const_ptr = &arg_const_ptr_value;

    // Wrap arguments
    entt::meta_any value_arg = arg_value;                         // int by value
    entt::meta_any ref_arg = entt::forward_as_meta(arg_ref);       // float& by non-const reference
    entt::meta_any const_ref_arg = entt::forward_as_meta(arg_const_ref); // const float& by const reference
    entt::meta_any ptr_arg = arg_ptr;                              // double* by non-const pointer
    entt::meta_any const_ptr_arg = arg_const_ptr;                  // const double* by const pointer

    // Assert storage policies to ensure correct wrapping
    EXPECT_EQ(ref_arg.base().policy(), entt::any_policy::ref) << "ref_arg should have 'ref' policy";
    EXPECT_EQ(const_ref_arg.base().policy(), entt::any_policy::cref) << "const_ref_arg should have 'cref' policy";
    EXPECT_EQ(ptr_arg.base().policy(), entt::any_policy::embedded) << "ptr_arg should have 'embedded' policy (pointers are copied)";
    EXPECT_EQ(const_ptr_arg.base().policy(), entt::any_policy::embedded) << "const_ptr_arg should have 'embedded' policy (pointers are copied)";

    // Call the function
    auto result_any = func_meta.invoke(
        instance,
        value_arg,
        entt::forward_as_meta(arg_ref),
        entt::forward_as_meta(arg_const_ref),
        ptr_arg,
        const_ptr_arg
    );
    ASSERT_TRUE(result_any) << "Invocation of 'mutate_and_sum' failed";

    // Expected post-call values
    float expected_b = 2.5f + 1.5f;   // b mutated inside function
    double expected_d = 7.5 + 2.5;    // *d mutated inside function
    // c and e are const, not mutated
    float expected_c = 4.5f;
    double expected_e = 5.5;

    int expected_result = 10 + static_cast<int>(expected_b) + static_cast<int>(expected_d) + static_cast<int>(expected_c) + static_cast<int>(expected_e); // 10 + 4 + 10 + 4 + 5 = 33

    // Verify return value
    EXPECT_EQ(result_any.cast<int>(), expected_result) << "Unexpected result from function";

    // Verify side effects
    EXPECT_FLOAT_EQ(arg_ref, expected_b) << "float& argument was not properly mutated";
    EXPECT_DOUBLE_EQ(arg_ptr_value, expected_d) << "double* argument was not properly mutated";

    // const ref and const pointer should be unchanged
    EXPECT_FLOAT_EQ(arg_const_ref, expected_c) << "const float& argument was unexpectedly modified";
    EXPECT_DOUBLE_EQ(arg_const_ptr_value, expected_e) << "const double* argument was unexpectedly modified";
}

TEST_F(MetaRegistrationTest, VerifyEnumMetaEntries)
{
    MockType::AnEnum enum_value;;
    auto enum_entries = gather_meta_enum_entries(enum_value);

    // Verify enum names and values
    ASSERT_FALSE(enum_entries.empty()) << "No entries found for the enum";
    ASSERT_EQ(enum_entries.size(), 3) << "Expected 3 entries for the enum";
    EXPECT_EQ(enum_entries[0].first, "Hello") << "First entry should be 'Hello'";
    EXPECT_EQ(enum_entries[0].second.cast<int>(), static_cast<int>(MockType::AnEnum::Hello)) << "First entry value should match enum value";
    EXPECT_EQ(enum_entries[1].first, "Bye") << "Second entry should be 'Bye'";
    EXPECT_EQ(enum_entries[1].second.cast<int>(), static_cast<int>(MockType::AnEnum::Bye)) << "Second entry value should match enum value";
    EXPECT_EQ(enum_entries[2].first, "Hola") << "Third entry should be 'Hola'";
    EXPECT_EQ(enum_entries[2].second.cast<int>(), static_cast<int>(MockType::AnEnum::Hola)) << "Third entry value should match enum value";
}