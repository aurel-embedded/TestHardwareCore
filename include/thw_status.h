/*
 * thw_status.h
 *
 *  Created on: 24 févr. 2026
 *      Author: AurelienPajadon
 */

#ifndef THW_STATUS_H
#define THW_STATUS_H

typedef enum
{
    THW_STATUS_OK = 0,
    THW_STATUS_ERROR,
    THW_STATUS_INVALID_PARAM,
    THW_STATUS_IO_ERROR,
    THW_STATUS_TIME_ERROR,
} thw_status_t;

#endif /* THW_STATUS_H */
