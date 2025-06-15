// Licensed under the MIT License. See LICENSE file for details.

// #include <iostream>
#include <entt/entt.hpp>
#include <entt/meta/pointer.hpp>
#include "nlohmann/json.hpp"
#include "config.h"
#include "MetaReg.hpp"
#include "ResourceTypes.h"
#include "MetaLiterals.h"
#include "ResourceRegistry.hpp" // Storage
#include "MetaInfo.h"

// namespace entt {
//   template<typename T>
//   struct is_meta_pointer_like<T*> : std::true_type {};
// }
// namespace entt {
//     template<>
//     struct is_meta_pointer_like<eeng::Storage*> : std::true_type {};
// }

namespace eeng {

    // + EngineContext for serialization & deserialization

#if 0
    void BehaviorScript_to_json(nlohmann::json& j, const void* ptr)
    {
        std::cout << "BehaviorScript_to_json\n";

        auto script = static_cast<const BehaviorScript*>(ptr);

        // self sol::table
        nlohmann::json self_json;
        table_to_json(self_json, script->self);
        j["self"] = self_json;

        j["identifier"] = script->identifier;
        j["path"] = script->path;
    }

    void BehaviorScript_from_json(
        const nlohmann::json& j,
        void* ptr,
        const Entity& entity,
        Editor::Context& context)
    {
        std::cout << "BehaviorScript_from_json\n";

        auto script = static_cast<BehaviorScript*>(ptr);
        std::string identifier = j["identifier"];
        std::string path = j["path"];

        // self sol::table
        // LOAD + copy Lua meta fields
        *script = BehaviorScriptFactory::create_from_file(
            *context.registry,
            entity,
            *context.lua, // self.lua_state() <- nothing here
            path,
            identifier);

        table_from_json(script->self, j["self"]);
    }
#endif

#if 0
    entt::meta<BehaviorScript>()
        .type("BehaviorScript"_hs).prop(display_name_hs, "BehaviorScript")

        .data<&BehaviorScript::identifier>("identifier"_hs)
        .prop(display_name_hs, "identifier")
        .prop(readonly_hs, true)

        .data<&BehaviorScript::path>("path"_hs)
        .prop(display_name_hs, "path")
        .prop(readonly_hs, true)

        // sol stuff
        .data<&BehaviorScript::self/*, entt::as_ref_t*/>("self"_hs).prop(display_name_hs, "self")
        .data<&BehaviorScript::update/*, entt::as_ref_t*/>("update"_hs).prop(display_name_hs, "update")
        .data<&BehaviorScript::on_collision/*, entt::as_ref_t*/>("on_collision"_hs).prop(display_name_hs, "on_collision")

        // inspect_hs
        // We have this, and not one for sol::table, so that when a sol::table if edited,
        // a deep copy of BehaviorScript is made, and not just the sol::table
        .func < [](void* ptr, Editor::InspectorState& inspector) {return Editor::inspect_type(*static_cast<BehaviorScript*>(ptr), inspector); } > (inspect_hs)

        // clone
        .func<&copy_BehaviorScript>(clone_hs)

        // to_json
        .func<&BehaviorScript_to_json>(to_json_hs)

        // from_json
        .func<&BehaviorScript_from_json>(from_json_hs)
        ;
#endif

    namespace
    {
        // class AssetDatabase;

        // template<typename T>
        // void serialize_handle(
        //     const Handle<T>& handle,
        //     nlohmann::json& j,
        //     AssetDatabase& adb)
        // {
        //     Guid guid = adb.get_guid(handle);
        //     j = guid.to_string();
        // }

        // template<typename T>
        // void deserialize_handle(
        //     Handle<T>& handle,
        //     const nlohmann::json& j,
        //     AssetDatabase& registry)
        // {
        //     Guid guid = Guid::from_string(j.get<std::string>());
        //     handle = registry.get_or_load<T>(guid);
        // }

        template<typename T>
        void register_handle(const std::string& name)
        {
            using handle_type = Handle<T>;

            entt::meta_factory<handle_type>()
                .type(entt::hashed_string{ name.c_str() })
                //.type(entt::hashed_string::value(name.c_str()))
                // .template func<&serialize_handle<T>>("serialize"_hs)
                // .template func<&deserialize_handle<T>>("deserialize"_hs)
                ;
        }

        template<class T>
        void assure_storage(eeng::Storage& storage)
        {
            storage.assure_storage<T>();
        }
    }

    void register_meta_types()
    {
        // === HANDLES ===

        // register_handle<MockResource1>("Handle<MockResource1>");
        // register_handle<MockResource2>("Handle<MockResource2>");

        // === RESOURCES ===

        // entt::meta<Texture>()
        //     .type("Texture"_hs)
        //     .func<&assure_storage<Texture>>("assure_storage"_hs);
        //
        // + forward_as_meta
        // auto assure_fn = entt::resolve<Texture>().func("assure_storage"_hs);
        // assure_fn.invoke({}, registry);

        entt::meta_factory<MockResource1>{}
        .type("MockResource1"_hs)
            .custom<TypeMetaInfo>(TypeMetaInfo{ "MockResource1", "This is a mock resource type." })

            // Register member 'x' with DisplayInfo and DataFlags
            .data<&MockResource1::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "An integer member 'x'" })
            .traits(MetaFlags::read_only)

            // Register member 'y' with DisplayInfo and multiple DataFlags
            .data<&MockResource1::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "A float member 'y'" })
            .traits(MetaFlags::hidden | MetaFlags::read_only)

            // Required for all resource types
            .template func<&assure_storage<MockResource1>>("assure_storage"_hs)

            //.func < [](eeng::Storage& storage) { } > (inspect_hs)
            //.data<&BehaviorScript::on_collision/*, entt::as_ref_t*/>("on_collision"_hs).prop(display_name_hs, "on_collision")

            ;
    }

} // namespace eeng