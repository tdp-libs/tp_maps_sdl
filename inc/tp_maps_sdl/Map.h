#ifndef tp_maps_sdl_Map_h
#define tp_maps_sdl_Map_h

#include "tp_maps_sdl/Globals.h"

#include "tp_maps/Map.h"

namespace tp_maps_sdl
{

//##################################################################################################
class TP_MAPS_SDL_SHARED_EXPORT Map : public tp_maps::Map
{
public:
  //################################################################################################
  Map(bool enableDepthBuffer = true, bool fullScreen = false, const std::string& title=std::string());

  //################################################################################################
  ~Map() override;

  //################################################################################################
  void exec();

  //################################################################################################
  void processEvents();

  //################################################################################################
  void makeCurrent() override;

  //################################################################################################
  //! Called to queue a refresh
  void update() override;

private:
  struct Private;
  Private* d;
  friend struct Private;
};
}
#endif
