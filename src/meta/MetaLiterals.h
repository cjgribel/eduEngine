//
//  meta_literals.h
//  cppmisc
//
//  Created by Carl Johan Gribel on 2024-08-11.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef meta_literals_h
#define meta_literals_h

#include <entt/entt.hpp>

using namespace entt::literals;

constexpr entt::hashed_string display_name_hs = "display_name"_hs;

constexpr entt::hashed_string to_string_hs = "to_string"_hs;

constexpr entt::hashed_string clone_hs = "clone"_hs;

constexpr entt::hashed_string to_json_hs = "to_json"_hs;
constexpr entt::hashed_string from_json_hs = "from_json"_hs;

constexpr entt::hashed_string inspect_hs = "inspect"_hs;

constexpr entt::hashed_string readonly_hs = "readonly"_hs;

constexpr bool ReadonlyDefault = false;

#endif /* meta_literals_h */
