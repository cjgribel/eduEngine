//
//  MetaInspect.hpp
//  engine_core_2024
//
//  Created by Carl Johan Gribel on 2024-08-08.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef MetaInspect_hpp
#define MetaInspect_hpp

#include <entt/entt.hpp>
#include "config.h"
#include "EditComponentCommand.hpp"

namespace Editor {

    class ComponentCommandBuilder;

    /// @brief Create a name for an entity suitable for imgui widgets
    /// @param registry 
    /// @param entity 
    /// @param comp_with_name_meta_data Meta type of a component with a "name" data field
    /// @return A string in format [entity id] or [name]##[entity id]
    std::string get_entity_name(
        const std::shared_ptr<const entt::registry>& registry,
        entt::entity entity,
        entt::meta_type meta_type_with_name);

    struct InspectorState;

    bool inspect_enum_any(
        entt::meta_any& any,
        InspectorState& inspector);

    bool inspect_any(
        entt::meta_any& any,
        InspectorState& inspector,
        ComponentCommandBuilder& cmd_builder);

    bool inspect_entity(
        const Entity& entity,
        InspectorState& inspector);

#if 0
    bool inspect_registry(
        entt::meta_type comp_with_name,
        InspectorState& inspector);
#endif

}

#endif /* MetaSerialize_hpp */
