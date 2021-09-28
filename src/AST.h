#pragma once

#include <Buffer.h>

struct ASTNode {
    enum class Type {
        Unit, // file, translation unit, whatever
        FunctionCall,
        StringLiteral,
        NumericLiteral,
        // Identifier,
    } type;
    LK::Buffer<ASTNode> children;
};
