#pragma once

namespace webframe::ORM::details
{
    template<typename T, typename C>
    class mem_ptr_utils<T C::*>
    {
    public:
        using class_type = C;
    };
}