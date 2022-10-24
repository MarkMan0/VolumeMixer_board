#include "mixer_gui.h"
#include "mixer_api.h"
#include "gfx.h"

static MixerAPI api;

void render_volume(const mixer::ProgramVolume&);

void mixer_gui_task(ISerial& uart) {
  api.set_uart(&uart);

  while (1) {
    if (not api.changes()) {
      vTaskDelay(500);
      continue;
    }

    if (MixerAPI::ret_t::OK != api.load_volumes()) {
      vTaskDelay(500);
      continue;
    }
    auto& volumes = api.get_volumes();

    if (not volumes[0]) {
      continue;
    }

    auto& curr = volumes[0].value();

    render_volume(curr);
    vTaskDelay(1000);
  }
}

static uint8_t img_buff[5000] = { 0 };

void render_volume(const mixer::ProgramVolume& vol) {
  gdispClear(GFX_BLACK);
  auto font = gdispOpenFont("DejaVuSans24*");

  gdispDrawString(10, 20, vol.name_, font, GFX_WHITE);

  char buff[50];

  snprintf(buff, 49, "PID: %d", vol.pid_);
  gdispDrawString(20, 44, buff, font, GFX_WHITE);

  snprintf(buff, 49, "Volume: %d%%", vol.volume_);
  gdispDrawString(20, 68, buff, font, GFX_WHITE);
  gdispCloseFont(font);

  gdispImage img;
  gdispImageInit(&img);
  img.width = 32;
  img.height = 32;
  img.bgcolor = GFX_BLACK;

  api.load_image(vol.pid_, img_buff, 5000);

  gdispImageOpenMemory(&img, img_buff);
  gdispImageDraw(&img, 20, 98, 32, 32, 0, 0);
  gdispImageClose(&img);
}