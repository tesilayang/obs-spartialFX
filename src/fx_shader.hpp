#pragma once
#include <string>

namespace spatial {
// OBS's effect parser discards global const initializers when generating shaders.
// Substitute the selector in function bodies before parsing, so D3D can eliminate
// sibling branches without a global constant or an unset shader parameter.
inline bool specializeFxShader(std::string &common, std::string &body, int selector, bool isolateBranches = false)
{
    // FX05 uses independent textures in different children. OBS assigns sampler
    // slots before D3D eliminates dead branches, so remove sibling blocks first.
    // These catalog bodies use flat, braced `if (effect_id == N)` child blocks.
    if (isolateBranches) {
        const std::string prefix = "if (effect_id == ";
        size_t at = 0;
        while ((at = body.find(prefix, at)) != std::string::npos) {
            const auto number = at + prefix.size();
            const auto open = body.find(") {", number);
            if (open == std::string::npos) return false;
            const bool keep = body.substr(number, open-number) == std::to_string(selector);
            size_t end = open + 3;
            int depth = 1;
            for (; end < body.size() && depth; ++end) {
                if (body[end] == '{') ++depth;
                else if (body[end] == '}') --depth;
            }
            if (depth) return false;
            if (keep) {
                body.erase(at, open+2-at);
                at = end-(open+2-at);
            } else body.erase(at, end-at);
        }
    }
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
