// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef meta_literals_h
#define meta_literals_h

#include <entt/entt.hpp>

using namespace entt::literals;

//constexpr entt::hashed_string display_name_hs = "display_name"_hs;

constexpr entt::hashed_string to_string_hs = "to_string"_hs;

constexpr entt::hashed_string clone_hs = "clone"_hs;

constexpr entt::hashed_string serialize_hs = "serialize"_hs;
constexpr entt::hashed_string deserialize_hs = "deserialize"_hs;

constexpr entt::hashed_string inspect_hs = "inspect"_hs;

constexpr entt::hashed_string assure_storage_hs = "assure_storage"_hs;

//constexpr entt::hashed_string readonly_hs = "readonly"_hs;

//constexpr bool ReadonlyDefault = false;

#endif /* meta_literals_h */
