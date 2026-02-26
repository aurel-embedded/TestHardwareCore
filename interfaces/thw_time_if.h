/*
 * thw_time_if.h
 *
 *  Created on: 25 févr. 2026
 *      Author: AurelienPajadon
 */

#ifndef THW_INTERFACES_THW_TIME_IF_H_
#define THW_INTERFACES_THW_TIME_IF_H_


#include <stdint.h>

typedef struct
{
    // Return a monotonic tick in milliseconds
    uint32_t (*getTickMs)(void);

} thw_time_if_t;

#endif /* THW_INTERFACES_THW_TIME_IF_H_ */
