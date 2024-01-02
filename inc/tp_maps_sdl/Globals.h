#ifndef tp_maps_sdl_Globals_h
#define tp_maps_sdl_Globals_h

#include "tp_maps/textures/BasicTexture.h" // IWYU pragma: keep

#if defined(TP_MAPS_SDL_LIBRARY)
#  define TP_MAPS_SDL_SHARED_EXPORT
#else
#  define TP_MAPS_SDL_SHARED_EXPORT
#endif


//##################################################################################################
//! An implementation of tp_maps running in SDL.
namespace tp_maps_sdl
{

//##################################################################################################
tp_image_utils::ColorMap loadTextureFromResource(const std::string& path);

}

#endif
