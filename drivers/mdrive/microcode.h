#ifndef MICROCODE_H
#define MICROCODE_H

extern int
mdrive_microcode_load(Driver * self, const char * filename);

extern int
mdrive_microcode_inspect(mdrive_device_t * device);

#endif
