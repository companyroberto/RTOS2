/*============================================================================
 * Autor:		Roberto Company
 * Fecha:		25/04/2018
 *===========================================================================*/

/*==================[inlcusiones]============================================*/

#include "sapi_datatypes.h"
#include "bufferCircular.h"

/*==================[definiciones de funciones internas]=====================*/

void inicializarbuffer( bufferCircular_t * bufferCircular )
{
	bufferCircular->escritura = 0;
	bufferCircular->lectura   = 0;
}

bool leerBuffer (bufferCircular_t * bufferCircular, uint8_t * dato)
{
	if ( bufferCircular->lectura == bufferCircular->escritura )
		return false;
	else {
		*dato = bufferCircular->buffer[ bufferCircular->lectura ];
		bufferCircular->lectura	= ( bufferCircular->lectura + 1 ) % L_BUFFER;
		return true;
	}
}

bool escribirBuffer (bufferCircular_t * bufferCircular, uint8_t dato)		//char Data
{
	if ( (bufferCircular->escritura + 1) % L_BUFFER == bufferCircular->lectura )
		return false;											// Write buffer is full
	else {
		bufferCircular->buffer[ bufferCircular->escritura ] = dato;
		bufferCircular->escritura = (bufferCircular->escritura + 1) % L_BUFFER;
		return true;
	}
}

void escribirBufferString (bufferCircular_t * bufferCircular, char* STR_PTR)
{
	char i = 0;
	while( STR_PTR [ i ] != '\0' )
	{
		escribirBuffer( bufferCircular, STR_PTR [ i ] );
		i++;
	}
}
