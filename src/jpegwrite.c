#include "proto.h"

void generate_JPEG_data (struct context * context) {
  if (context -> source -> frames > 1) throw(context, PLUM_ERR_NO_MULTI_FRAME);
  if ((context -> source -> width > 0xffffu) || (context -> source -> height > 0xffffu)) throw(context, PLUM_ERR_IMAGE_TOO_LARGE);
  byteoutput(context,
             0xff, 0xd8, // SOI
             0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x02, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, // JFIF marker (no thumbnail)
             0xff, 0xee, 0x00, 0x0e, 0x41, 0x64, 0x6f, 0x62, 0x65, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x01, // Adobe marker (YCbCr colorspace)
             0xff, 0xc0, 0x00, 0x11, 0x08, // SOF, baseline DCT coding, 8 bits per component...
             context -> source -> height >> 8, context -> source -> height, context -> source -> width >> 8, context -> source -> width, // dimensions...
             0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01 // 3 components, component 1 is 4:4:4, table 0, components 2-3 are 4:2:0, table 1
            );
  uint8_t luminance_table[64];
  uint8_t chrominance_table[64];
  calculate_JPEG_quantization_tables(context, luminance_table, chrominance_table);
  unsigned char * node = append_output_node(context, 134);
  bytewrite(node, 0xff, 0xdb, 0x00, 0x84, 0x00); // DQT, 132 bytes long, table 0 first
  memcpy(node + 5, luminance_table, sizeof luminance_table);
  node[69] = 1; // table 1 afterwards
  memcpy(node + 70, chrominance_table, sizeof chrominance_table);
  size_t unitsH = (context -> image -> width + 7) / 8, unitsV = (context -> image -> height + 7) / 8, units = unitsH * unitsV;
  double (* luminance)[64] = ctxmalloc(context, units * sizeof *luminance);
  double (* blue_chrominance)[64] = ctxmalloc(context, units * sizeof *blue_chrominance);
  double (* red_chrominance)[64] = ctxmalloc(context, units * sizeof *red_chrominance);
  convert_JPEG_components_to_YCbCr(context, luminance, blue_chrominance, red_chrominance);
  size_t reduced_units = ((unitsH + 1) >> 1) * ((unitsV + 1) >> 1);
  double (* buffer)[64] = ctxmalloc(context, reduced_units * sizeof *buffer);
  subsample_JPEG_component(blue_chrominance, buffer, unitsH, unitsV);
  ctxfree(context, blue_chrominance);
  blue_chrominance = buffer;
  buffer = ctxmalloc(context, reduced_units * sizeof *buffer);
  subsample_JPEG_component(red_chrominance, buffer, unitsH, unitsV);
  ctxfree(context, red_chrominance);
  red_chrominance = buffer;
  size_t luminance_count, chrominance_count;
  // do chrominance first, since it will generally use less memory, so the chrominance data can be freed afterwards to reduce overall memory usage
  struct JPEG_encoded_value * chrominance_data = generate_JPEG_chrominance_data_stream(context, blue_chrominance, red_chrominance, reduced_units, chrominance_table,
                                                                                       &chrominance_count);
  ctxfree(context, red_chrominance);
  ctxfree(context, blue_chrominance);
  struct JPEG_encoded_value * luminance_data = generate_JPEG_luminance_data_stream(context, luminance, units, luminance_table, &luminance_count);
  ctxfree(context, luminance);
  unsigned char Huffman_table_data[0x400]; // luminance DC, AC, chrominance DC, AC
  node = append_output_node(context, 1096);
  *node = 0xff;
  node[1] = 0xc4; // DHT
  size_t size = 4;
  size += generate_JPEG_Huffman_table(context, luminance_data, luminance_count, node + size, Huffman_table_data, 0x00);
  size += generate_JPEG_Huffman_table(context, luminance_data, luminance_count, node + size, Huffman_table_data + 0x100, 0x10);
  size += generate_JPEG_Huffman_table(context, chrominance_data, chrominance_count, node + size, Huffman_table_data + 0x200, 0x01);
  size += generate_JPEG_Huffman_table(context, chrominance_data, chrominance_count, node + size, Huffman_table_data + 0x300, 0x11);
  node[2] = (size - 2) >> 8;
  node[3] = size - 2;
  context -> output -> size = size;
  byteoutput(context, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x00); // SOS, component 1, table 0, not progressive
  encode_JPEG_scan(context, luminance_data, luminance_count, Huffman_table_data);
  ctxfree(context, luminance_data);
  byteoutput(context, 0xff, 0xda, 0x00, 0x0a, 0x02, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00); // SOS, components 2-3, table 1, not progressive
  encode_JPEG_scan(context, chrominance_data, chrominance_count, Huffman_table_data + 0x200);
  ctxfree(context, chrominance_data);
  byteoutput(context, 0xff, 0xd9); // EOI
}

void calculate_JPEG_quantization_tables (struct context * context, uint8_t luminance_table[restrict static 64], uint8_t chrominance_table[restrict static 64]) {
  // start with the standard's tables (reduced by 1, since that will be added back later)
  static const uint8_t luminance_base[64] =   { 15,  10,  11,  13,  11,   9,  15,  13,  12,  13,  17,  16,  15,  18,  23,  39,  25,  23,  21,  21,  23,
                                                48,  34,  36,  28,  39,  57,  50,  60,  59,  56,  50,  55,  54,  63,  71,  91,  77,  63,  67,  86,  68,
                                                54,  55,  79, 108,  80,  86,  94,  97, 102, 103, 102,  61,  76, 112, 120, 111,  99, 119,  91, 100, 102,  98};
  static const uint8_t chrominance_base[64] = { 16,  17,  17,  23,  20,  23,  46,  25,  25,  46,  98,  65,  55,  65,  98,  98,  98,  98,  98,  98,  98,
                                                98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,
                                                98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98,  98};
  // compute a score based on the logarithm of the image's dimensions (approximated using integer math)
  uint_fast32_t current, score = 0;
  for (current = context -> source -> width; current > 4; current >>= 1) score += 2;
  score += current;
  for (current = context -> source -> height; current > 4; current >>= 1) score += 2;
  score += current;
  if (score < 24)
    score = 2;
  else
    score -= 22;
  // adjust the chrominance accuracy based on the color depth
  current = get_true_color_depth(context -> source);
  uint_fast32_t adjustment = 72 - (current & 0xff) - ((current >> 8) & 0xff) - ((current >> 16) & 0xff);
  // compute the final quantization coefficients based on the scores above
  for (current = 0; current < 64; current ++) {
    luminance_table[current] = 1 + luminance_base[current] * score / 25;
    chrominance_table[current] = 1 + chrominance_base[current] * score * adjustment / 1200;
  }
}

void convert_JPEG_components_to_YCbCr (struct context * context, double (* restrict luminance)[64], double (* restrict blue)[64], double (* restrict red)[64]) {
  const unsigned char * data = context -> source -> data;
  size_t offset = context -> source -> palette ? 1 : plum_color_buffer_size(1, context -> source -> color_format), rowoffset = offset * context -> source -> width;
  size_t unitrow, unitcol, row, col;
  double palette_luminance[256];
  double palette_blue[256];
  double palette_red[256];
  // define macros to reduce repetition within the function
  #define nextunit luminance ++, blue ++, red ++
  #define convertblock(rows, cols) do                                                                                               \
    if (context -> source -> palette)                                                                                               \
      for (row = 0; row < (rows); row ++) for (col = 0; col < (cols); col ++) {                                                     \
        unsigned char index = data[(unitrow * 8 + row) * context -> source -> width + unitcol * 8 + col], coord = row * 8 + col;    \
        coord[*luminance] = palette_luminance[index];                                                                               \
        coord[*blue] = palette_blue[index];                                                                                         \
        coord[*red] = palette_red[index];                                                                                           \
      }                                                                                                                             \
    else {                                                                                                                          \
      size_t index = unitrow * 8 * rowoffset + unitcol * 8 * offset;                                                                \
      for (row = 0; row < (rows); row ++, index += rowoffset)                                                                       \
        convert_JPEG_colors_to_YCbCr(context, data + index, (cols), context -> source -> color_format, *luminance + 8 * row,        \
                                     *blue + 8 * row, *red + 8 * row);                                                              \
    }                                                                                                                               \
  while (0)
  #define copyvalues(index, offset) do {                     \
    unsigned char coord = (index);                           \
    (*luminance)[coord] = (*luminance)[coord - (offset)];    \
    (*blue)[coord] = (*blue)[coord - (offset)];              \
    (*red)[coord] = (*red)[coord - (offset)];                \
  } while (0)
  // actually do the conversion
  if (context -> source -> palette)
    convert_JPEG_colors_to_YCbCr(context, context -> source -> palette, context -> source -> max_palette_index + 1, context -> source -> color_format,
                                 palette_luminance, palette_blue, palette_red);
  for (unitrow = 0; unitrow < (context -> source -> height >> 3); unitrow ++) {
    for (unitcol = 0; unitcol < (context -> source -> width >> 3); unitcol ++) {
      convertblock(8, 8);
      nextunit;
    }
    if (context -> source -> width & 7) {
      convertblock(8, context -> source -> width & 7);
      for (row = 0; row < 8; row ++) for (col = context -> source -> width & 7; col < 8; col ++) copyvalues(row * 8 + col, 1);
      nextunit;
    }
  }
  if (context -> source -> height & 7) {
    for (unitcol = 0; unitcol < (context -> source -> width >> 3); unitcol ++) {
      convertblock(context -> source -> height & 7, 8);
      for (col = 8 * (context -> source -> height & 7); col < 64; col ++) copyvalues(col, 8);
      nextunit;
    }
    if (context -> source -> width & 7) {
      convertblock(context -> source -> height & 7, context -> source -> width & 7);
      for (row = 0; row < (context -> source -> height & 7); row ++) for (col = context -> source -> width & 7; col < 8; col ++) copyvalues(row * 8 + col, 1);
      for (col = 8 * (context -> source -> height & 7); col < 64; col ++) copyvalues(col, 8);
    }
  }
  #undef copyvalues
  #undef convertblock
  #undef nextunit
}

void convert_JPEG_colors_to_YCbCr (struct context * context, const void * colors, size_t count, unsigned char flags, double * restrict luminance,
                                   double * restrict blue, double * restrict red) {
  uint64_t * buffer = ctxmalloc(context, sizeof *buffer * count);
  plum_convert_colors(buffer, colors, count, PLUM_COLOR_64, flags);
  size_t p;
  for (p = 0; p < count; p ++) {
    double R = (double) (buffer[p] & 0xffffu) / 257.0, G = (double) ((buffer[p] >> 16) & 0xffffu) / 257.0, B = (double) ((buffer[p] >> 32) & 0xffffu) / 257.0;
    luminance[p] = 0x0.4c8b4395810628p+0 * R + 0x0.9645a1cac08310p+0 * G + 0x0.1d2f1a9fbe76c8p+0 * B - 128.0;
    blue[p] = 0.5 * (B - 1.0) - 0x0.2b32468049f7e8p+0 * R - 0x0.54cdb97fb60818p+0 * G;
    red[p] = 0.5 * (R - 1.0) - 0x0.6b2f1c1ead19ecp+0 * G - 0x0.14d0e3e152e614p+0 * B;
  }
  ctxfree(context, buffer);
}

void subsample_JPEG_component (double (* restrict component)[64], double (* restrict output)[64], size_t unitsH, size_t unitsV) {
  size_t unitrow, unitcol, row, col, p;
  #define reduce(offset, shift) do {                                                             \
    const double * ref = component[(offset) * unitsH] + (row * 16 + col * 2 - 64 * (offset));    \
    (*output)[row * 8 + col + (shift)] = (*ref + ref[1] + ref[8] + ref[9]) * 0.25;               \
  } while (0)
  for (unitrow = 0; unitrow < (unitsV >> 1); unitrow ++) {
    for (unitcol = 0; unitcol < (unitsH >> 1); unitcol ++) {
      for (p = 0; p < 8; p += 4) {
        for (row = 0; row < 4; row ++) for (col = 0; col < 4; col ++) reduce(0, p);
        for (; row < 8; row ++) for (col = 0; col < 4; col ++) reduce(1, p);
        component ++;
      }
      output ++;
    }
    if (unitsH & 1) {
      for (row = 0; row < 4; row ++) for (col = 0; col < 4; col ++) reduce(0, 0);
      for (; row < 8; row ++) for (col = 0; col < 4; col ++) reduce(1, 0);
      component ++;
      for (row = 0; row < 8; row ++) for (col = 4; col < 8; col ++) (*output)[row * 8 + col] = (*output)[row * 8 + col - 1];
      output ++;
    }
    component += unitsH; // skip odd rows
  }
  if (unitsV & 1) {
    for (unitcol = 0; unitcol < (unitsH >> 1); unitcol ++) {
      for (p = 0; p < 8; p += 4) {
        for (row = 0; row < 4; row ++) for (col = 0; col < 4; col ++) reduce(0, p);
        component ++;
      }
      for (p = 32; p < 64; p ++) (*output)[p] = (*output)[p - 8];
      output ++;
    }
    if (unitsH & 1) {
      for (row = 0; row < 4; row ++) for (col = 0; col < 4; col ++) {
        reduce(0, 0);
        (*output)[row * 8 + col + 4] = (*output)[row * 8 + col];
      }
      for (p = 32; p < 64; p ++) (*output)[p] = (*output)[p - 8];
    }
  }
  #undef reduce
}
