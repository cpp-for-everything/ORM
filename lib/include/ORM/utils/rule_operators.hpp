#pragma once

namespace webframe::ORM::details
{
    enum rule_operators 
    {
        Less, // <
        Greater, // >
        Equals, // ==
        Not_equal, // !=
        Greater_or_equal, // >=
        Less_or_equal, // <=
        And, // &&
        Or, // ||
        Xor, // ^
    };
} // namespace details
