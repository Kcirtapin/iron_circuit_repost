#ifndef IRON_CIRCUIT_RENDER_RENDERER_H
#define IRON_CIRCUIT_RENDER_RENDERER_H

#include "render/DrawQueue.h"
#include "util/Types.h"
#include <SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

class Renderer
{
public:
    explicit Renderer(SDL_Renderer *sdlRenderer);
    ~Renderer();

    DrawQueue *getDrawQueue() { return &m_drawQueue; }
    const DrawQueue *getDrawQueue() const { return &m_drawQueue; }
    void setCameraFocus(util::Vector2i focusTile, int levelWidth, int levelHeight);
    int cameraOffsetX() const { return m_cameraOffsetX; }
    int cameraOffsetY() const { return m_cameraOffsetY; }
    void clear();
    void renderAll();
    void present();

private:
    bool isWorldLayer(util::DrawLayer layer) const;
    Rect transformWorldRect(const Rect &src) const;
    SDL_Texture *loadTexture(const std::string &relativePath);
    std::string resolveAssetPath(const std::string &relativePath) const;
    TTF_Font *fontForSize(int ptSize);

    SDL_Renderer *m_sdlRenderer;
    DrawQueue m_drawQueue;
    TTF_Font *m_font = nullptr;
    std::string m_fontPath;
    std::unordered_map<int, TTF_Font *> m_fontCache;
    std::unordered_map<std::string, SDL_Texture *> m_textureCache;
    int m_cameraOffsetX = 0;
    int m_cameraOffsetY = 0;
};

#endif // IRON_CIRCUIT_RENDER_RENDERER_H