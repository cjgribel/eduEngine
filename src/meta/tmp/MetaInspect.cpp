//
//  MetaInspect.cpp
//  engine_core_2024
//
//  Created by Carl Johan Gribel on 2024-08-08.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#include <iostream>
#include <sstream>
#include <cassert>
#include "imgui.h"
#include "meta_literals.h"
#include "meta_aux.h"

#include "MetaClone.hpp"
#include "MetaInspect.hpp"
#include "InspectType.hpp"
#include "EditComponentCommand.hpp"

namespace Editor {

    namespace
    {
        void generate_command(auto& cmd_queue_wptr, auto& cmd_builder)
        {
            assert(!cmd_queue_wptr.expired());
            auto cmd_queue_sptr = cmd_queue_wptr.lock();
            cmd_queue_sptr->add(CommandFactory::Create<ComponentCommand>(cmd_builder.build()));
        }
    }

    std::string get_entity_name(
        const std::shared_ptr<const entt::registry>& registry,
        entt::entity entity,
        entt::meta_type meta_type_with_name)
    {
        //assert(registry.valid(entity));
        auto entity_str = std::to_string(entt::to_integral(entity));

#ifdef EENG_DEBUG
        if (!registry->valid(entity)) entity_str = entity_str + " [invalid]";
#endif

        // No meta type to use
        if (!meta_type_with_name) return entity_str;

        // Check if name data field exists
        entt::meta_data meta_data = meta_type_with_name.data("name"_hs);
        if (!meta_data) return entity_str;

        // Find storage for component type
        auto storage = registry->storage(meta_type_with_name.id());
        if (!storage) return entity_str;
        if (!storage->contains(entity)) return entity_str;

        // Instantiate component
        auto v = storage->value(entity);
        auto comp_any = meta_type_with_name.from_void(v);
        if (!comp_any) return entity_str;

        // Get data value
        auto data = meta_data.get(comp_any);

        // Fetch name from component
        auto name_ptr = data.template try_cast<std::string>();
        // Cast failed
        if (!name_ptr) return entity_str;

        // Does NOT return the correct string
//        return *name_ptr + std::string("###") + entity_str;

        return data.template cast<std::string>() + "###" + entity_str;
    }

    bool inspect_enum_any(
        entt::meta_any& any,
        InspectorState& inspector)
    {
        entt::meta_type meta_type = entt::resolve(any.type().id());
        assert(meta_type);
        assert(meta_type.is_enum());

        // Cast to underlying enum meta type
        auto any_conv = cast_to_underlying_type(meta_type, any);

        // Gather enum entries and determine which is the current one
        auto enum_entries = gather_meta_enum_entries(any);
        auto cur_entry = std::find_if(enum_entries.begin(), enum_entries.end(), [&any_conv](auto& e) {
            return e.second == any_conv;
            });
        assert(cur_entry != enum_entries.end());

        // Build combo
        auto selected_entry = cur_entry;
        if (ImGui::BeginCombo("##enumcombo", cur_entry->first.c_str()))
        {
            for (auto it = enum_entries.begin(); it != enum_entries.end(); it++)
            {
                const bool isSelected = (it->second == cur_entry->second);
                if (ImGui::Selectable(it->first.c_str(), isSelected))
                    selected_entry = it;

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Update current enum if needed
        if (selected_entry != cur_entry)
        {
            // assert(any.assign(selected_entry->second));
            bool ret = any.assign(selected_entry->second);
            assert(ret);
            return true;
        }
        return false;
    }

    bool inspect_any(
        entt::meta_any& any,
        InspectorState& inspector,
        ComponentCommandBuilder& cmd_builder)
    {
        assert(any);
        bool mod = false; // TODO: not used yet
#ifdef INSPECTION_DEBUG_PRINTS
        std::cout << "inspect_any " << meta_type_name(any.type()) << std::endl;
#endif

        if (entt::meta_type meta_type = entt::resolve(any.type().id()); meta_type)
        {
            if (entt::meta_func meta_func = meta_type.func(inspect_hs); meta_func)
            {
                // Inspection meta function signatures:
                //      bool(void* ptr, Editor::InspectorState& inspector)
                //      bool(const void* ptr, Editor::InspectorState& inspector) ?

#ifdef USE_COMMANDS
                // Invoke inspection meta function on a copy of the object

                auto copy_any = Editor::clone_any(any, inspector.entity_selection.first());
                //auto copy_any = any;
                auto res_any = meta_func.invoke({}, copy_any.data(), entt::forward_as_meta(inspector));
                assert(res_any && "Failed to invoke inspect meta function");

                // Issue command if a change is detected
                auto res_ptr = res_any.try_cast<bool>();
                assert(res_ptr && "inspect meta function is expected to return bool");
                if (*res_ptr)
                {
                    // Build & issue command
                    cmd_builder.prev_value(any).new_value(copy_any);
                    generate_command(inspector.cmd_queue, cmd_builder);

                    mod = true;
                }
#else
                // Invoke the inspection meta function on the object
                auto res_any = meta_func.invoke({}, any.data(), entt::forward_as_meta(inspector));
                assert(res_any && "Failed to invoke inspect meta function");
                auto res_ptr = res_any.try_cast<bool>();
                assert(res_ptr && "inspect meta function expected to return bool");
                mod |= *res_ptr;
#endif
            }
            else if (meta_type.is_enum())
            {
#ifdef USE_COMMANDS
                auto copy_any = any;
                if (inspect_enum_any(copy_any, inspector))
                {
                    // Build & issue command
                    cmd_builder.prev_value(any).new_value(copy_any);
                    generate_command(inspector.cmd_queue, cmd_builder);

                    mod = true;
                }
#else
                mod |= inspect_enum_any(any, inspector);
#endif
            }
            else
            {
                // inspect() not available: traverse data members
                for (auto&& [id, meta_data] : meta_type.data())
                {
                    std::string key_name = meta_data_name(id, meta_data);
#ifdef INSPECTION_DEBUG_PRINT
                    std::cout << "inspecting data field: " << key_name << std::endl;
#endif
                    // Open the next node unless it is a container
                    ImGui::SetNextItemOpen(true);
                    // {
                    //     bool data_is_container = false;
                    //     data_is_container |= meta_data.type().is_associative_container();
                    //     data_is_container |= meta_data.type().is_sequence_container();
                    //     ImGui::SetNextItemOpen(!data_is_container);
                    // }

                    if (inspector.begin_node(key_name.c_str()))
                    {
#ifdef USE_COMMANDS
                        // Push command meta path
                        cmd_builder.push_path_data(id, key_name);
#endif
                        // Obtain copy of data value
                        entt::meta_any data_any = meta_data.get(any); //.as_ref() will yield REF to a TEMP VALUE if entt::as_ref_t is not used
                        //std::cout << key_name << ": is_ref " << (any.policy() == entt::meta_any_policy::ref) << ", " << (data_any.policy() == entt::meta_any_policy::ref) << std::endl;
                        // Check & set readonly
                        bool readonly = get_meta_data_prop<bool, ReadonlyDefault>(meta_data, readonly_hs);
                        if (readonly) inspector.begin_disabled();

                        // Inspect
                        mod |= inspect_any(data_any, inspector, cmd_builder);
#ifndef USE_COMMANDS
                        // Update data of the current object
                        meta_data.set(any, data_any);
#endif
                        // Unset readonly
                        if (readonly) inspector.end_disabled();
#ifdef USE_COMMANDS
                        // Pop command meta path
                        cmd_builder.pop_path();
#endif
                        inspector.end_node();
                    }
#ifdef INSPECTION_DEBUG_PRINT
                    std::cout << "DONE inspecting data field" << key_name << std::endl;
#endif
                }
            }
            return mod;
        }

        // any is not a meta type

        if (any.type().is_sequence_container())
        {
            auto view = any.as_sequence_container();
            assert(view && "as_sequence_container() failed");
#ifdef INSPECTION_DEBUG_PRINT
            std::cout << "is_sequence_container: " << meta_type_name(any.type()) << ", size " << view.size() << std::endl;
#endif
            int count = 0;
            for (auto&& v : view)
            {
                ImGui::SetNextItemOpen(true);
                inspector.begin_leaf((std::string("#") + std::to_string(count)).c_str());
                {
#ifdef USE_COMMANDS
                    // Push command meta path
                    cmd_builder.push_path_index(count, std::to_string(count));

                    // ImGui::SetNextItemWidth(-FLT_MIN);
                    mod |= inspect_any(v, inspector, cmd_builder);

                    // Pop command meta path
                    cmd_builder.pop_path();
#else
                    // ImGui::SetNextItemWidth(-FLT_MIN);
                    mod |= inspect_any(v, inspector, cmd_builder); // Will change the actual element
#endif
                }
                inspector.end_leaf();
                count++;
            }
        }

        else if (any.type().is_associative_container())
        {
            auto view = any.as_associative_container();
            assert(view && "as_associative_container() failed");
#ifdef INSPECTION_DEBUG_PRINT
            std::cout << "is_associative_container: " << meta_type_name(any.type()) << ", size " << view.size() << std::endl;
#endif
            int count = 0;
            for (auto&& [key_any, mapped_any] : view)
            {
                // Displaying key, value and the element itself as expandable nodes
                ImGui::SetNextItemOpen(true);
                if (inspector.begin_node((std::string("#") + std::to_string(count)).c_str()))
                {
                    ImGui::SetNextItemOpen(true);
                    if (inspector.begin_node("[key]"))
                    {
                        // Note: keys are read-only, and primitive key types
                        // will cast to const (and display as disabled if a 
                        // const specialization is defined). When meta types 
                        // are used as keys however, this does not seem to work
                        // the same way (they cast to non-const - but cannot be
                        // modified).
                        // Workaround: disable all key inspections explicitly
                        inspector.begin_disabled();
                        inspect_any(key_any, inspector, cmd_builder); // key_any holds a const object
                        inspector.end_disabled();
                        inspector.end_node();
                    }
                    if (view.mapped_type())
                    {
#ifdef USE_COMMANDS
                        // Push command meta path
                        cmd_builder.push_path_key(key_any, std::to_string(count));
#endif
                        // Inspect mapped value
                        ImGui::SetNextItemOpen(true);
                        if (inspector.begin_node("[value]"))
                        {
                            mod |= inspect_any(mapped_any, inspector, cmd_builder);
                            inspector.end_node();
                        }
#ifdef USE_COMMANDS
                        // Pop command meta path
                        cmd_builder.pop_path();
#endif
                    }
                    inspector.end_node();
                }
                count++;
            }
        }

        else //if (any.policy() != entt::meta_any_policy::cref)
        {

            // Copy as any before???
            // entt::meta_any any_copy = Editor::clone_any(any, inspector.selected_entity);

            // Try casting the meta_any to a primitive type and perform the inspection
            bool res = try_apply(any, [&](auto& value) {
#ifdef USE_COMMANDS
                // Inspect a copy of the value and issue command if a change is detected
                auto value_copy = value;

                //entt::meta_any any_copy = Editor::clone_any(any, inspector.selected_entity);

                // std::remove_reference_t<decltype(value)> v;
                //decltype(value) v;

                if (Editor::inspect_type(value_copy, inspector))
                {
                    // Build & issue command
                    cmd_builder.prev_value(any).new_value(value_copy);
                    generate_command(inspector.cmd_queue, cmd_builder);

                    mod = true;
                }
#else
                // Inspect the value
                Editor::inspect_type(value, inspector);
#endif
                });
            if (!res)
                throw std::runtime_error(std::string("Unable to cast type ") + meta_type_name(any.type()));
        }
        // else { /* cref */ }

#ifdef INSPECTION_DEBUG_PRINT
        std::cout << "DONE inspect_any " << meta_type_name(any.type()) << std::endl;
#endif
        return mod;
    }

    bool inspect_entity(
        const Entity& entity,
        InspectorState& inspector)
    {
        bool mod = false;
        ComponentCommandBuilder cmd_builder;

        auto& registry = inspector.context.registry;
        assert(!entity.is_null());
        assert(registry->valid(entity));

        for (auto&& [id, type] : registry->storage())
        {
            if (!type.contains(entity)) continue;

            if (entt::meta_type meta_type = entt::resolve(id); meta_type)
            {
                auto type_name = meta_type_name(meta_type);

                if (inspector.begin_node(type_name.c_str()))
                {
#ifdef USE_COMMANDS
                    // Reset meta command for component type
                    cmd_builder.reset().
                        registry(inspector.context.registry)
                        .entity(entity)
                        .component(id);
#endif
                    auto comp_any = meta_type.from_void(type.value(entity)); // ref
                    mod |= inspect_any(comp_any, inspector, cmd_builder);

                    inspector.end_node();
                }
            }
            else
            {
                //All types exposed to Lua are going to have a meta type
                assert(false && "Meta-type required");
            }
        }

        return mod;
    }

#if 0
    bool inspect_registry(
        entt::meta_type name_comp_type,
        InspectorState& inspector)
    {
        bool mod = false;
        auto& registry = inspector.context.registry;

        auto view = registry->template view<entt::entity>();
        for (auto entity : view)
        {
#if 0
            inspect_entity(entity, inspector);
#else
            auto entity_name = get_entity_name(registry, entity, name_comp_type);
            if (!inspector.begin_node(entity_name.c_str()))
                continue;

            for (auto&& [id, type] : registry->storage())
            {
                if (!type.contains(entity)) continue;

                if (entt::meta_type meta_type = entt::resolve(id); meta_type)
                {
                    auto type_name = meta_type_name(meta_type);

                    if (inspector.begin_node(type_name.c_str()))
                    {
                        auto comp_any = meta_type.from_void(type.value(entity));
                        mod |= inspect_any(comp_any, inspector);
                        inspector.end_node();
                    }
                }
                else
                {
                    //All types exposed to Lua are going to have a meta type
                    assert(false && "Meta-type required");
                }
            }
            inspector.end_node();
#endif
        }
        return mod;
    }
#endif

} // namespace Editor