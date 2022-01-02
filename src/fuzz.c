#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include "libplum.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  unsigned error = 0;
  struct plum_image * img = plum_load_image(Data, Size, 0, &error);
  assert((img != 0) != (error != 0));

  // TODO plum_store_image with randomly selected type, to malloc'd buffer or size

  if (img) {
    plum_destroy_image(img);
  }

  return 0;  // Non-zero return values are reserved for future use.
}
