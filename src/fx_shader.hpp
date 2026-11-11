#pragma once
#include <string>

namespace spatial {
// OBS's effect parser discards global const initializers when generating shaders.
// Substitute the selector in function bodies before parsing, so D3D can eliminate
// sibling branches without a global constant or an unset shader parameter.
inline bool specializeFxShader(std::string &common, std::string &body, int selector)
{
    const std::string declaration = "uniform int effect_id;";
    const auto declarationAt = common.find(declaration);
    if (declarationAt == std::string::npos) return false;
    common.erase(declarationAt, declaration.size());
    const std::string token = "effect_id";
    const auto value = std::to_string(selector);
    size_t at = 0;
    while ((at = body.find(token, at)) != std::string::npos) {
        body.replace(at, token.size(), value);
        at += value.size();
    }
    return true;
}
}
