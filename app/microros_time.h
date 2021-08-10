#ifndef MICROROS_TIME__H
#define MICROROS_TIME__H

#include <time.h>

int clock_gettime(clockid_t t, struct timespec * tspec);

#endif  // MICROROS_TIME__H
