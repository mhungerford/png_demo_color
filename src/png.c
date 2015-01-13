#include "upng.h"
#include "png.h"

#define MAX(A,B) ((A>B) ? A : B)
#define MIN(A,B) ((A<B) ? A : B)

#define TO_RGB8(color) \
  (color.r & 0xC0) | ((color.g & 0xC0) >> 2) | ((color.b & 0xC0) >> 4) | 0x03

inline static char reverse_byte(uint8_t input) {
#if defined(__arm__)
  uint8_t result;
  __asm__ ("rev  %[input], %[result]\n\t"
           "rbit %[result], %[result]"
           : [result] "=r" (result)
           : [input] "r" (input));
  return result;
#else
  return ((input * 0x0202020202ULL & 0x010884422010ULL) % 1023);
#endif
}

static bool gbitmap_from_bitmap(
    GBitmap* gbitmap, const uint8_t* bitmap_buffer, int width, int height, int bpp) {
  // Copy width and height to GBitmap
  gbitmap->bounds.origin.x = 0;
  gbitmap->bounds.origin.y = 0;
  gbitmap->bounds.size.w = width;
  gbitmap->bounds.size.h = height;
  gbitmap->is_heap_allocated = true;  // allows gbitmap_destroy to cleanup

  // GBitmap must be word-aligned, so we need to copy from bitmap to 
  // word-aligned GBitmap buffer
  if (bpp == 1) {
    gbitmap->format = GBitmapFormat1Bit;
    // 1-bit GBitmap needs to be word aligned per line (bytes)
    gbitmap->row_size_bytes = ((width + 31) / 32 ) * 4;
    gbitmap->addr = malloc(height * gbitmap->row_size_bytes); 
    if (gbitmap->addr == NULL) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "malloc gbitmap->addr failed");
    }
    for(int y = 0; y < height; y++) {
      memcpy(
          &(((uint8_t*)gbitmap->addr)[y * gbitmap->row_size_bytes]), 
          &(bitmap_buffer[y * ((width + 7) / 8)]), 
          (width + 7) / 8);
    }
    // GBitmap pixels are most-significant bit, so need to flip each byte.
    for(int i = 0; i < gbitmap->row_size_bytes * height; i++){
      ((uint8_t*)gbitmap->addr)[i] = reverse_byte(((uint8_t*)gbitmap->addr)[i]);
    }
  } else if (bpp == 8) {
    gbitmap->format = GBitmapFormat8Bit;
    gbitmap->row_size_bytes = width;
    gbitmap->addr = (unsigned char*)bitmap_buffer;
  }

  return true;
}

upng_t* upng_decompression_wrapper(uint8_t *png_raw_buffer, int png_raw_size) {
  upng_t *upng = upng_new_from_bytes(png_raw_buffer, png_raw_size);
  if (upng == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG malloc error"); 
  }
  if (upng_get_error(upng) != UPNG_EOK) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG Loaded:%d line:%d", 
      upng_get_error(upng), upng_get_error_line(upng));
  }
  if (upng_decode(upng) != UPNG_EOK) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG Decode:%d line:%d", 
      upng_get_error(upng), upng_get_error_line(upng));
  }

  if (upng_get_format(upng) >= UPNG_INDEXED1 || upng_get_format(upng) <= UPNG_INDEXED8) {
    //Decode paletized image to raw rgb values
    unsigned int width = upng_get_width(upng);
    unsigned int height = upng_get_height(upng);
    unsigned int bpp = upng_get_bpp(upng);
    uint8_t *upng_buffer = (uint8_t*)upng_get_buffer(upng);

    //rgb palette
    rgb *palette = NULL;
    upng_get_palette(upng, &palette);

    printf("bpp was %d\n", bpp);

    //convert paletized image to rgb8
    for (unsigned int i = 0; i < upng_get_height(upng) * upng_get_width(upng); i++) {
      if (bpp == 4) {
        uint8_t *rgb_buffer = malloc(width * height * upng_get_bpp(upng) / 8);
        rgb_buffer[i] = TO_RGB8(palette[upng_buffer[((i + 1) / 2)]]);
        upng_buffer = rgb_buffer;
        // TODO: fix this because of CCM
      } else if (bpp == 8) {
        // In-place palette to rgb conversion
        upng_buffer[i] = TO_RGB8(palette[upng_buffer[i]]);
      }
    }
  }
  return upng;
}

GBitmap* gbitmap_create_with_png_resource(uint32_t resource_id) {
  //Allocate gbitmap
  GBitmap* gbitmap_ptr = malloc(sizeof(GBitmap));

  ResHandle rHdl = resource_get_handle(resource_id);
  int png_raw_size = resource_size(rHdl);
  uint8_t* png_raw_buffer = malloc(png_raw_size); //freed by upng impl
  if (png_raw_buffer == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "malloc png resource buffer failed");
  }
  resource_load(rHdl, png_raw_buffer, png_raw_size);

  upng_t* upng = upng_decompression_wrapper(png_raw_buffer, png_raw_size);

  gbitmap_from_bitmap(gbitmap_ptr, upng_get_buffer(upng),
    upng_get_width(upng), upng_get_height(upng), 8);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "converted to gbitmap");

  // Free the png, no longer needed
  upng_free(upng);
  upng = NULL;

  return gbitmap_ptr;
}

//Note: this frees the source data during decoding to save memory usage
GBitmap* gbitmap_create_with_png_data(uint8_t *data, int data_bytes) {
  //Allocate gbitmap
  GBitmap* gbitmap_ptr = malloc(sizeof(GBitmap));

  upng_t* upng = upng_decompression_wrapper(data, data_bytes);

  gbitmap_from_bitmap(gbitmap_ptr, upng_get_buffer(upng),
    upng_get_width(upng), upng_get_height(upng), 8);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "converted to gbitmap");

  // Free the png, no longer needed
  upng_free(upng);
  upng = NULL;

  return gbitmap_ptr;
}

