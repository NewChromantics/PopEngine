#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data);

#ifdef __cplusplus
}
#endif