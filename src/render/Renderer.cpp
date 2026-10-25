#include "render/Renderer.h"
#include "render/DrawQueue.h"
#include "util/GameConfig.h"
#include "util/Types.h"
#include <SDL.h>
#include <SDL_ttf.h>
#if defined(IRON_CIRCUIT_HAS_SDL_IMAGE) && IRON_CIRCUIT_HAS_SDL_IMAGE
#include <SDL_image.h>
#endif
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

Renderer::Renderer(SDL_Renderer *sdlRenderer) : m_sdlRenderer(sdlRenderer)
{
    SDL_RenderSetLogicalSize(m_sdlRenderer, util::layout::LogicalWidth, util::layout::LogicalHeight);

    if (TTF_WasInit() == 0 && TTF_Init() != 0)
    {
        std::cerr << "[Renderer] TTF_Init failed: " << TTF_GetError() << "\n";
    }

    std::vector<std::string> candidates;
    if (char *basePath = SDL_GetBasePath())
    {
        const std::filesystem::path base(basePath);
        candidates.push_back((base / "resource" / "fonts" / "RobotoMono-Regular.ttf").string());
        candidates.push_back((base / ".." / "resource" / "fonts" / "RobotoMono-Regular.ttf").string());
        SDL_free(basePath);
    }
    candidates.push_back((std::filesystem::path("resource") / "fonts" / "RobotoMono-Regular.ttf").string());
    candidates.push_back((std::filesystem::path("..") / "resource" / "fonts" / "RobotoMono-Regular.ttf").string());

    for (const auto &path : candidates)
    {
        m_font = TTF_OpenFont(path.c_str(), 22);
        if (m_font) {
            m_fontPath = path;
            break;
        }
    }

    if (!m_font)
    {
        std::cerr << "[Renderer] TTF_OpenFont failed for all paths: " << TTF_GetError() << "\n";
    }
}

Renderer::~Renderer()
{
    for (auto &entry : m_textureCache)
    {
        if (entry.second)
            SDL_DestroyTexture(entry.second);
    }
    m_textureCache.clear();

    if (m_font)
    {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }
    for (auto &entry : m_fontCache)
    {
        if (entry.second)
            TTF_CloseFont(entry.second);
    }
    m_fontCache.clear();
}

bool Renderer::isWorldLayer(util::DrawLayer layer) const
{
    return layer != util::DrawLayer::UI_OVERLAY;
}

Rect Renderer::transformWorldRect(const Rect &src) const
{
    return Rect{
        src.x - m_cameraOffsetX + util::layout::WorldViewportX,
        src.y - m_cameraOffsetY + util::layout::WorldViewportY,
        src.w,
        src.h};
}

std::string Renderer::resolveAssetPath(const std::string &relativePath) const
{
    std::vector<std::filesystem::path> candidates;
    if (char *basePath = SDL_GetBasePath())
    {
        const std::filesystem::path base(basePath);
        candidates.push_back(base / relativePath);
        candidates.push_back(base / ".." / relativePath);
        SDL_free(basePath);
    }
    candidates.push_back(std::filesystem::path(relativePath));
    candidates.push_back(std::filesystem::path("..") / relativePath);

    for (const auto &candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
            return candidate.string();
    }
    return std::string();
}

SDL_Texture *Renderer::loadTexture(const std::string &relativePath)
{
    if (relativePath.empty())
        return nullptr;

    const auto cached = m_textureCache.find(relativePath);
    if (cached != m_textureCache.end())
        return cached->second;

    SDL_Texture *texture = nullptr;
    const std::string resolvedPath = resolveAssetPath(relativePath);
    if (resolvedPath.empty())
    {
        m_textureCache[relativePath] = nullptr;
        return nullptr;
    }

#if defined(IRON_CIRCUIT_HAS_SDL_IMAGE) && IRON_CIRCUIT_HAS_SDL_IMAGE
    texture = IMG_LoadTexture(m_sdlRenderer, resolvedPath.c_str());
#else
    SDL_Surface *surface = SDL_LoadBMP(resolvedPath.c_str());
    if (surface)
    {
        texture = SDL_CreateTextureFromSurface(m_sdlRenderer, surface);
        SDL_FreeSurface(surface);
    }
#endif

    if (!texture)
    {
        std::cerr << "[Renderer] Failed to load sprite '" << relativePath << "'\n";
    }

    m_textureCache[relativePath] = texture;
    return texture;
}

TTF_Font *Renderer::fontForSize(int ptSize)
{
    if (ptSize <= 0)
        ptSize = 22;
    if (ptSize == 22)
        return m_font;
    const auto found = m_fontCache.find(ptSize);
    if (found != m_fontCache.end())
        return found->second;
    if (m_fontPath.empty())
        return m_font;
    TTF_Font *font = TTF_OpenFont(m_fontPath.c_str(), ptSize);
    if (!font)
        return m_font;
    m_fontCache[ptSize] = font;
    return font;
}

void Renderer::setCameraFocus(util::Vector2i focusTile, int levelWidth, int levelHeight)
{
    const int worldPixelWidth = levelWidth * util::layout::CellSize;
    const int worldPixelHeight = levelHeight * util::layout::CellSize;
    const int targetX = focusTile.x * util::layout::CellSize + util::layout::CellSize / 2 - util::layout::WorldViewportW / 2;
    const int targetY = focusTile.y * util::layout::CellSize + util::layout::CellSize / 2 - util::layout::WorldViewportH / 2;

    m_cameraOffsetX = std::max(0, targetX);
    m_cameraOffsetY = std::max(0, targetY);

    const int maxCameraX = std::max(0, worldPixelWidth - util::layout::WorldViewportW);
    const int maxCameraY = std::max(0, worldPixelHeight - util::layout::WorldViewportH);
    m_cameraOffsetX = std::min(m_cameraOffsetX, maxCameraX);
    m_cameraOffsetY = std::min(m_cameraOffsetY, maxCameraY);
}

void Renderer::clear()
{
    SDL_SetRenderDrawColor(m_sdlRenderer, 14, 18, 28, 255);
    SDL_RenderClear(m_sdlRenderer);

    SDL_SetRenderDrawColor(m_sdlRenderer, 22, 28, 40, 255);
    SDL_Rect worldBg{util::layout::WorldViewportX, util::layout::WorldViewportY,
                     util::layout::WorldViewportW, util::layout::WorldViewportH};
    SDL_RenderFillRect(m_sdlRenderer, &worldBg);
}

void Renderer::renderAll()
{
    std::vector<DrawRequest> requests = m_drawQueue.getSorted();
    SDL_Rect worldClip{util::layout::WorldViewportX, util::layout::WorldViewportY,
                       util::layout::WorldViewportW, util::layout::WorldViewportH};

    for (const auto &req : requests)
    {
        const bool worldLayer = isWorldLayer(req.layer);
        const Rect drawRect = worldLayer ? transformWorldRect(req.dst) : req.dst;

        SDL_Rect clipRect;
        SDL_Rect drawSdlRect{drawRect.x, drawRect.y, drawRect.w, drawRect.h};
        const bool hasBounds = drawRect.w > 0 && drawRect.h > 0;

        if (worldLayer)
        {
            clipRect = worldClip;
            SDL_RenderSetClipRect(m_sdlRenderer, &clipRect);
        }
        else if (hasBounds)
        {
            clipRect = drawSdlRect;
            SDL_RenderSetClipRect(m_sdlRenderer, &clipRect);
        }
        else
        {
            SDL_RenderSetClipRect(m_sdlRenderer, nullptr);
        }

        SDL_Texture *resolvedTexture = req.texture;
        if (!resolvedTexture && !req.texturePath.empty())
            resolvedTexture = loadTexture(req.texturePath);

        if (resolvedTexture)
        {
            SDL_Rect *srcRect = nullptr;
            SDL_Rect src{};
            if (req.src.w > 0 && req.src.h > 0)
            {
                src = {req.src.x, req.src.y, req.src.w, req.src.h};
                srcRect = &src;
            }
            SDL_Rect dst = {drawRect.x, drawRect.y, drawRect.w, drawRect.h};
            SDL_RenderCopy(m_sdlRenderer, resolvedTexture, srcRect, &dst);
        }
        else if (req.usePlaceholderShape)
        {
            Uint8 a = (req.placeholderColor >> 24) & 0xFF;
            Uint8 r = (req.placeholderColor >> 16) & 0xFF;
            Uint8 g = (req.placeholderColor >> 8) & 0xFF;
            Uint8 b = req.placeholderColor & 0xFF;
            SDL_SetRenderDrawBlendMode(m_sdlRenderer,
                                       a < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(m_sdlRenderer, r, g, b, a);
            SDL_Rect rect = {drawRect.x, drawRect.y, drawRect.w, drawRect.h};
            if (req.outlineOnly)
                SDL_RenderDrawRect(m_sdlRenderer, &rect);
            else
                SDL_RenderFillRect(m_sdlRenderer, &rect);
        }

        TTF_Font *reqFont = fontForSize(req.textPointSize);
        if (req.hasText && !req.text.empty() && reqFont)
        {
            Uint8 a = (req.textColor >> 24) & 0xFF;
            Uint8 r = (req.textColor >> 16) & 0xFF;
            Uint8 g = (req.textColor >> 8) & 0xFF;
            Uint8 b = req.textColor & 0xFF;
            SDL_Color color{r, g, b, a};
            const int boxW = std::max(24, drawRect.w > 0 ? drawRect.w : util::layout::SidebarW - 32);
            const int boxH = std::max(24, drawRect.h > 0 ? drawRect.h : 48);
            SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(reqFont, req.text.c_str(), color, static_cast<Uint32>(boxW));
            if (!surface)
                continue;
            SDL_Texture *tex = SDL_CreateTextureFromSurface(m_sdlRenderer, surface);
            if (!tex)
            {
                SDL_FreeSurface(surface);
                continue;
            }

            const float scaleX = static_cast<float>(boxW) / static_cast<float>(std::max(1, surface->w));
            const float scaleY = static_cast<float>(boxH) / static_cast<float>(std::max(1, surface->h));
            const float scale = std::min(1.0f, std::min(scaleX, scaleY));
            const int fittedW = std::max(1, static_cast<int>(surface->w * scale));
            const int fittedH = std::max(1, static_cast<int>(surface->h * scale));

            if (worldLayer)
            {
                SDL_RenderSetClipRect(m_sdlRenderer, &worldClip);
            }
            else if (hasBounds)
            {
                SDL_RenderSetClipRect(m_sdlRenderer, &drawSdlRect);
            }
            else
            {
                SDL_RenderSetClipRect(m_sdlRenderer, nullptr);
            }

            SDL_Rect dst{drawRect.x, drawRect.y, fittedW, fittedH};
            SDL_RenderCopy(m_sdlRenderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surface);
        }
    }

    SDL_RenderSetClipRect(m_sdlRenderer, nullptr);
    SDL_SetRenderDrawBlendMode(m_sdlRenderer, SDL_BLENDMODE_NONE);
}

void Renderer::present()
{
    SDL_RenderPresent(m_sdlRenderer);
}