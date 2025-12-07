/* Stub for training functionality to satisfy linker */
/* We do not support training in the Postgres extension to avoid external
 * dependencies like liblbfgs */

#include <stddef.h>

int crf1de_create_instance(const char *interface, void **ptr) {
  /* Always fail to create training instance */
  return 1;
}
