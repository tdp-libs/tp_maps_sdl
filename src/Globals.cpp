#include "tp_maps_sdl/Globals.h"

#include "tp_utils/DebugUtils.h"
#include "tp_utils/Resources.h"

#include <SDL2/SDL_image.h>

namespace tp_maps_sdl
{

//##################################################################################################
tp_maps::TextureData loadTextureFromResource(const std::string& path)
{
  if(!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
  {
    tpWarning() << "SDL_image could not initialize! SDL_image Error: " << IMG_GetError();
    return tp_maps::TextureData();
  }

  tp_utils::Resource resource = tp_utils::resource(path);
  if(!resource.data || resource.size<1)
  {
    tpWarning() << "Failed to load resource: " << path;
    return tp_maps::TextureData();
  }

  auto rw = SDL_RWFromConstMem(resource.data, int(resource.size));
  if(!rw)
  {
    tpWarning() << "Failed to create SDL_RWops for resource: " << path;
    return tp_maps::TextureData();
  }
  TP_CLEANUP([&]{SDL_FreeRW(rw);});

  auto surface = IMG_Load_RW(rw, 0);
  if(!surface)
  {
    tpWarning() << "Failed to create SDL_Surface for resource: " << path;
    return tp_maps::TextureData();
  }
  TP_CLEANUP([&]{SDL_FreeSurface(surface);});

  SDL_LockSurface(surface);
  TP_CLEANUP([&]{SDL_UnlockSurface(surface);});

  tp_maps::TextureData result;
  result.w = size_t(surface->w);
  result.h = size_t(surface->h);
  auto dst = new TPPixel[result.w*result.h];
  result.data = dst;

  std::ptrdiff_t pitch = surface->pitch / 4;
  auto hh = surface->h-1;
  for(int y=0; y<surface->h; y++)
  {
    // Note that:
    // tp_maps::TextureData 0,0 is in the bottom left.
    // SDL_Surface          0,0 is in the top left.
    const Uint32* src = reinterpret_cast<const Uint32*>(surface->pixels) + (pitch*(hh-y));
    auto srcMax = src+std::ptrdiff_t(surface->w);
    for(; src<srcMax; src++, dst++)
      SDL_GetRGBA(*src, surface->format, &dst->r, &dst->g, &dst->b, &dst->a);
  }

  return result;
}

}

