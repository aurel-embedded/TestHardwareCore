/*
 * thw_transport_if.h
 *
 *  Created on: 24 févr. 2026
 *      Author: AurelienPajadon
 */

#ifndef THW_TRANSPORT_IF_H
#define THW_TRANSPORT_IF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    bool (*init)(void);
    bool (*deinit)(void);

    // Blocking send (internal protection)
    bool (*write)(const uint8_t* data, size_t len);

    // Non-blocking read
    size_t (*read)(uint8_t* buffer, size_t maxLen);

    // Optional flush
    void (*flush)(void);

} thw_transport_if_t;

#endif

