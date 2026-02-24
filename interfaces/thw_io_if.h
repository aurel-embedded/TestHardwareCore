/*
 * thw_io_if.h
 *
 *  Created on: 24 févr. 2026
 *      Author: AurelienPajadon
 */

#ifndef THW_INTERFACES_THW_IO_IF_H_
#define THW_INTERFACES_THW_IO_IF_H_

#include <stdbool.h>
#include <stddef.h>


typedef struct
{
    bool (*init)(void);
    void (*deinit)(void);

    bool (*readLine)(char* buffer, size_t maxLen);
    void (*write)(const char* text);

} thw_io_if_t;


#endif /* THW_INTERFACES_THW_IO_IF_H_ */
