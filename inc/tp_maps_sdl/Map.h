#ifndef tp_maps_sdl_Map_h
#define tp_maps_sdl_Map_h

#include "tp_maps_sdl/Globals.h"

#include "tp_maps/Map.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>

namespace tp_maps_sdl
{

//##################################################################################################
class TP_MAPS_SDL_SHARED_EXPORT Map : public tp_maps::Map
{
  TP_DQ;
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
  void update(tp_maps::RenderFromStage renderFromStage=tp_maps::RenderFromStage::Full) override;

  //################################################################################################
  void callAsync(const std::function<void()>& callback) override;

  //################################################################################################
  void setRelativeMouseMode(bool enabled) override;

  //################################################################################################
  bool relativeMouseMode() const override;

  //################################################################################################
  void startTextInput() override;

  //################################################################################################
  void stopTextInput() override;
};
}
#endif
