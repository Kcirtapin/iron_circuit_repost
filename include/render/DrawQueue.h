#ifndef IRON_CIRCUIT_RENDER_DRAWQUEUE_H
#define IRON_CIRCUIT_RENDER_DRAWQUEUE_H

#include "util/Types.h"
#include <algorithm>
#include <string>
#include <vector>

struct SDL_Texture;

struct Rect { int x=0, y=0, w=0, h=0; };

struct DrawRequest {
    util::DrawLayer layer = util::DrawLayer::BACKGROUND;
    bool usePlaceholderShape = false;
    unsigned placeholderColor = 0;
    bool outlineOnly = false;
    Rect dst;
    int zOrder = 0;
    SDL_Texture* texture = nullptr;
    std::string texturePath;
    Rect src = {0, 0, 0, 0};
    bool hasText = false;
    std::string text;
    unsigned textColor = 0xFFFFFFFFu;
    int textPointSize = 22;
};

class DrawQueue {
public:
    void push(const DrawRequest& r) { m_reqs.push_back(r); }
    void clear() { m_reqs.clear(); }
    const std::vector<DrawRequest>& requests() const { return m_reqs; }
    std::vector<DrawRequest> getSorted() const {
        std::vector<DrawRequest> out = m_reqs;
        std::sort(out.begin(), out.end(), [](const DrawRequest& a, const DrawRequest& b) {
            if (static_cast<int>(a.layer) != static_cast<int>(b.layer))
                return static_cast<int>(a.layer) < static_cast<int>(b.layer);
            return a.zOrder < b.zOrder;
        });
        return out;
    }
private:
    std::vector<DrawRequest> m_reqs;
};

#endif // IRON_CIRCUIT_RENDER_DRAWQUEUE_H