//
//  meta_aux.h
//
//  Created by Carl Johan Gribel on 2024-08-08.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef meta_reg_h
#define meta_reg_h

#include "Context.hpp"
//#include <sol/forward.hpp>
// #include <entt/entt.hpp>
// #include <tuple>
// #include <utility>
// #include <cassert>

/// @brief 
/// @tparam Type 
/// @param lua 
template<class Type>
void register_meta(Editor::Context& context)
{
    assert(0 && "Specialization not available for type");
}

using ClassDataModifiedCallbackType = std::function<void(entt::meta_any)>;

#endif /* meta_reg */
