//
//  MetaClone.cpp
//  engine_core_2024
//
//  Created by Carl Johan Gribel on 2024-08-08.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#include <iostream>
#include <sstream>
#include <cassert>
#include "imgui.h"
#include "MetaInspect.hpp"
#include "InspectorState.hpp"
#include "meta_literals.h"
#include "meta_aux.h"

namespace Editor {

    /*

    */
    // src_any can be a component or a non-component
    // if type is meta & has clone_hs: use it; else value-copy src_any
    entt::meta_any clone_any(const entt::meta_any& any, entt::entity dst_entity)
    {
        if (entt::meta_type meta_type = entt::resolve(any.type().id()); meta_type)
        {
            if (entt::meta_func meta_func = meta_type.func(clone_hs); meta_func)
            {
                // std::cout << "clone_any: invoking clone() for " << any.type().info().name() << std::endl;

                auto copy_any = meta_func.invoke({}, any.data() /*src_ptr*/, dst_entity);
                assert(copy_any && "Failed to invoke clone() for type ");

                //type.push(dst_entity, copy_any.data());
                //continue;
                return copy_any;
            }
        }

        // std::cout << "clone_any: copying by value: " << any.type().info().name() << std::endl;
        return any;
    }

    // If a component is a meta type: use clone_hs if available; else use direct copy

    void clone_entity(std::shared_ptr<entt::registry>& registry, entt::entity src_entity, entt::entity dst_entity)
    {
        for (auto&& [id, type] : registry->storage())
        {
            if (!type.contains(src_entity)) continue;
            void* src_ptr = type.value(src_entity);
#if 1
            if (entt::meta_type meta_type = entt::resolve(id); meta_type)
            {
                auto comp_any = meta_type.from_void(type.value(src_entity)); // ref
                // mod |= inspect_any(comp_any, inspector, cmd_builder);
                auto copy_any = clone_any(comp_any, dst_entity); assert(copy_any);

                type.push(dst_entity, copy_any.data());
            }
            else
            {
                // Fallback: Direct copy if no meta type
                type.push(dst_entity, src_ptr);
            }
#else
            // Use clone() if it exists
            if (entt::meta_type meta_type = entt::resolve(id); meta_type)
            {

                if (entt::meta_func meta_func = meta_type.func(clone_hs); meta_func)
                {
                    std::cout << "Invoking clone() for " << type.type().name() << std::endl;

                    auto copy_any = meta_func.invoke({}, src_ptr, dst_entity);
                    assert(copy_any && "Failed to invoke clone() for type ");

                    type.push(dst_entity, copy_any.data());
                    continue;
                }
            }

            // Fallback: Direct copy if no meta type or clone function
            type.push(dst_entity, src_ptr);
#endif
        }
    }

} // namespace Editor