#ifndef tp_maps_sdl_Vulkan_h
#define tp_maps_sdl_Vulkan_h

#include "tp_maps_sdl/Globals.h" // IWYU pragma: keep

struct SDL_Window;

namespace tp_maps_sdl
{

//##################################################################################################
class Vulkan
{
  TP_DQ;
public:

  //################################################################################################
  Vulkan(SDL_Window* window, const std::string& title);

  //################################################################################################
  ~Vulkan();
};

}

#endif
