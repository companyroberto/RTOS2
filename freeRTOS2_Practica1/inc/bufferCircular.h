/*============================================================================
 * Autor:		Roberto Company
 * Fecha:		25/04/2018
 *===========================================================================*/

#ifndef _BUFFER_CIRCULAR_H_
#define _BUFFER_CIRCULAR_H_

#include "sapi_datatypes.h"

#define L_BUFFER 500

typedef struct {
	uint8_t buffer[L_BUFFER];
	uint8_t	lectura;
	uint8_t	escritura;
} bufferCircular_t;

bufferCircular_t bufferCircular;

void inicializarbuffer		( bufferCircular_t * bufferCircular );
bool leerBuffer 			( bufferCircular_t * bufferCircular, uint8_t * dato );
bool escribirBuffer 		( bufferCircular_t * bufferCircular, uint8_t dato );
void escribirBufferString	( bufferCircular_t * bufferCircular, char * STR_PTR );

#endif
