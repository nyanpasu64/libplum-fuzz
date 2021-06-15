#define PLUM_FILENAME (-(size_t) 1)
#define PLUM_BUFFER   (-(size_t) 2)

enum plum_flags {
  /* color formats */
  PLUM_COLOR_32   = 0, /* RGBA 8.8.8.8 */
  PLUM_COLOR_64   = 1, /* RGBA 16.16.16.16 */
  PLUM_COLOR_16   = 2, /* RGBA 5.5.5.1 */
  PLUM_COLOR_32X  = 3, /* RGBA 10.10.10.2 */
  PLUM_COLOR_MASK = 3,
  /* alpha */
  PLUM_ALPHA_INVERT = 4,
  PLUM_ALPHA_REMOVE = 8,
  /* palettes */
  PLUM_NO_PALETTE       =    0,
  PLUM_LOAD_PALETTE     = 0x10,
  PLUM_GENERATE_PALETTE = 0x20,
  PLUM_FORCE_PALETTE    = 0x30,
  PLUM_PALETTE_MASK     = 0x30
  /* ... */
};

enum plum_image_types {
  PLUM_IMAGE_BMP = 1
  /* ... */
};

enum plum_metadata_types {
  PLUM_METADATA_NONE,
  PLUM_METADATA_COLOR_DEPTH
  /* ... */
};

enum plum_errors {
  PLUM_OK,
  PLUM_ERR_INVALID_ARGUMENTS,
  PLUM_ERR_INVALID_FILE_FORMAT,
  PLUM_ERR_FILE_INACCESSIBLE,
  PLUM_ERR_FILE_ERROR,
  PLUM_ERR_OUT_OF_MEMORY,
  PLUM_ERR_TOO_MANY_COLORS,
  PLUM_ERR_INVALID_COLOR_INDEX,
  PLUM_ERR_IMAGE_TOO_LARGE,
  /* ... */
  PLUM_NUM_ERRORS
};
