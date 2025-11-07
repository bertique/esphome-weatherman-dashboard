#pragma once

#include "esphome/components/display/display_buffer.h"
#include "waveshare_epaper.h"

namespace esphome {
namespace waveshare_epaper {

class WaveshareEPaper7in5h : public WaveshareEPaper {
 public:
  void deep_sleep() override;
  void display() override;
  void initialize() override;
  void fill(Color color) override;

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void dump_config() override;
  void update() override;
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

 protected:
  int get_width_internal() override { return 800; }
  int get_height_internal() override { return 480; }

  // uint8_t *ry_buffer_;
  uint32_t get_buffer_length_() override; 
 private:
  void command(uint8_t value);
  void data(uint8_t value);
  void wait_until_idle();
  void reset();
  void turn_on_display();
};

}  // namespace waveshare_epaper
}  // namespace esphome