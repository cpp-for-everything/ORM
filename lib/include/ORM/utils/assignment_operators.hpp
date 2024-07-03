#pragma once

namespace webframe::ORM::details
{
    enum assignment_operators 
    {
        Eq, // =
        PlusEq, // +=
        MinusEq, // -=
        MulEq, // *=
        DivEq, // /=
        ModEq, // %=
    };
} // namespace details
