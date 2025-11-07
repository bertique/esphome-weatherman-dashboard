#include "waveshare_7in5h.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_7in5h";

void WaveshareEPaper7in5h::dump_config() {
  LOG_DISPLAY("", "Waveshare 7.5in (H)", this);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7in5h::initialize() {
  ESP_LOGD(TAG, "Allocating buffer...");
  
}

// All helper functions remain the same
void WaveshareEPaper7in5h::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}

void WaveshareEPaper7in5h::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

void WaveshareEPaper7in5h::wait_until_idle() {
  // This function waits for HIGH, which is correct for this controller.
  while (this->busy_pin_->digital_read() == 0) {
    delay(100);
    App.feed_wdt();
  }
}

void WaveshareEPaper7in5h::reset() {
    if (this->reset_pin_ != nullptr) {
        this->reset_pin_->digital_write(true);
        delay(200);
        this->reset_pin_->digital_write(false);
        delay(2);
        this->reset_pin_->digital_write(true);
        delay(200);
    }
}

void WaveshareEPaper7in5h::update() {
  ESP_LOGI(TAG, "Performing update...");
  
  // This is the missing call. It executes the lambda from your YAML configuration,
  // which populates the display buffer with your content by calling the drawing functions.
  this->do_update_();

  // 1. Perform hardware reset.
  this->reset();

  // 2. THIS IS THE CORRECT INITIALIZATION SEQUENCE FROM ARDUINO_R4
  this->command(0x00);
  this->data(0x0F);
  this->data(0x29);

  this->command(0x06);
  this->data(0x0F);
  this->data(0x8B);
  this->data(0x93);
  this->data(0xA1);

  this->command(0x41);
  this->data(0x00);

  this->command(0x50);
  this->data(0x37);

  this->command(0x60);
  this->data(0x02);
  this->data(0x02);

  this->command(0x61);
  this->data(this->get_width_internal() / 256);
  this->data(this->get_width_internal() % 256);
  this->data(this->get_height_internal() / 256);
  this->data(this->get_height_internal() % 256);

  this->command(0x62);
  this->data(0x98);
  this->data(0x98);
  this->data(0x98);
  this->data(0x75);
  this->data(0xCA);
  this->data(0xB2);
  this->data(0x98);
  this->data(0x7E);

  this->command(0x65);
  this->data(0x00);
  this->data(0x00);
  this->data(0x00);
  this->data(0x00);

  this->command(0xE7);
  this->data(0x1C);

  this->command(0xE3);
  this->data(0x00);

  this->command(0xE9);
  this->data(0x01);

  this->command(0x30);
  this->data(0x08);

  this->command(0x04); // Power ON
  this->wait_until_idle();
  ESP_LOGI(TAG, "Initialization complete. Screen should flash.");

  // 3. Send the image data.
  ESP_LOGI(TAG, "Sending image data...");
  this->command(0x10);
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++) {
    this->data(this->buffer_[i]);
    if (i % 512 == 0) App.feed_wdt();
  }
  
  // 4. Trigger the display refresh and sleep.
  ESP_LOGI(TAG, "Refreshing display...");
  this->display();
  this->deep_sleep();
  ESP_LOGI(TAG, "Update complete.");
}

void WaveshareEPaper7in5h::display() {
  this->command(0x12); // DISPLAY_REFRESH
  this->data(0x00);
  this->wait_until_idle();
}

void WaveshareEPaper7in5h::deep_sleep() {
    this->command(0x02); // POWER_OFF
    this->data(0X00);
    this->wait_until_idle();
    this->command(0x07); // DEEP_SLEEP
    this->data(0XA5);
}

// This new fill method is the key to fixing the background color.
void WaveshareEPaper7in5h::fill(Color color) {
  ESP_LOGD(TAG, "fill(color=(R:%d, G:%d, B:%d, W:%d))",
           color.red, color.green, color.blue, color.white);
  
  uint8_t color_val;
  // This logic correctly interprets RGB colors from ESPHome.
  if (color.red > 200 && color.green > 200 && color.blue < 50) {
    color_val = 0x02; // Yellow
  } else if (color.red > 200 && color.green < 50 && color.blue < 50) {
    color_val = 0x03; // Red
  } else if (color.red < 50 && color.green < 50 && color.blue < 50) {
    // A check for dark colors, which we will treat as black.
    color_val = 0x00; // Black
  } else {
    // Anything else (including pure white) will be treated as white.
    color_val = 0x01; // White
  }

  // This is the pixel-packing logic from the working Arduino EPD_7IN5H_Clear function.
  // It creates a byte where all four pixels have the same color.
  uint8_t packed_byte = (color_val << 6) | (color_val << 4) | (color_val << 2) | color_val;

  for (uint32_t i = 0; i < this->get_buffer_length_(); i++) {
    this->buffer_[i] = packed_byte;
  }
}

// Corrected buffer length for 2 bits per pixel.
uint32_t WaveshareEPaper7in5h::get_buffer_length_() {
    return (this->get_width_internal() * this->get_height_internal()) / 4;
}

// Corrected drawing function for 2 bits per pixel.
void HOT WaveshareEPaper7in5h::draw_absolute_pixel_internal(int x, int y, Color color) {
  //ESP_LOGV(TAG, "draw_absolute_pixel_internal(x=%d, y=%d, color=(R:%d, G:%d, B:%d, W:%d))", x, y, color.red,
  //         color.green, color.blue, color.white);
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  uint8_t color_val;
  // This logic correctly interprets the 4 colors this display supports.
  // It checks the RGB values from the ESPHome Color object.
  if (color.red > 200 && color.green > 200 && color.blue < 50) {
    color_val = 0x02; // Yellow
  } else if (color.red > 200 && color.green < 50 && color.blue < 50) {
    color_val = 0x03; // Red
  } else if (color.red < 50 && color.green < 50 && color.blue < 50) {
    // A check for dark colors, which we will treat as black.
    color_val = 0x00; // Black
  } else {
    // Anything else (including pure white) will be treated as white.
    color_val = 0x01; // White
  }

  uint32_t pos = (x + y * this->get_width_internal()) / 4;
  uint8_t shift = 6 - (x % 4) * 2;
  
  // Clear the 2 bits for the pixel first
  this->buffer_[pos] &= ~(0b11 << shift);
  // Set the new 2-bit color value
  this->buffer_[pos] |= (color_val << shift);
}

}  // namespace waveshare_epaper
}  // namespace esphome
