/*
 * evento.h
 *
 *  Created on: 11/05/2012
 *      Author: alejandro
 */

#ifndef EVENTOS_H_
#define EVENTOS_H_

#include "modulos.h"
#include "seniales.h"

void taskDespacharEventos			( void * colaEventos );
void EncolarEvento						( Modulo_t * receptor, Signal_t senial, int valor );
portBASE_TYPE EncolarEventoFromISR 	( Modulo_t * receptor, Signal_t senial, int valor );

#endif /* EVENTO_H_ */
