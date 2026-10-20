#include "ui/splash.h"
#include "display/display.h"
#include "render/boot.h"
#include "render/watch_display.h"
#include "util/flint_sys.h"

void splashShow() {
  if (!displayHasCanvas())
    return;
  const auto started = flintMillis();
  unsigned last = 0;
  do {
    const auto elapsed = unsigned(flintMillis() - started);
    if (!last || elapsed - last >= 40) {
      watchPresentScene(FlintRender::bootScene(elapsed));
      last = elapsed;
    }
    flintDelay(4);
  } while (unsigned(flintMillis() - started) < FlintRender::BootDurationMs);
  watchPresentScene(FlintRender::bootScene(FlintRender::BootDurationMs));
  watchFadeInNextScene();
}
