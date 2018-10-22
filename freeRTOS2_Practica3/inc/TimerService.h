/*
 * timeout.h
 *
 *  Created on: 29/11/2011
 *      Author: alejandro
 */

#ifndef TIMEOUT_H_
#define TIMEOUT_H_

//Operaciones que se pueden hacer con un timer
void timerArmarUnico		( Modulo_t * modulo, unsigned int timeout );
void timerArmarRepetitivo	( Modulo_t * modulo, unsigned int timeout );
void timerRecargar			( Modulo_t * modulo );
void timerDesarmar			( Modulo_t * modulo );

#endif /* TIMEOUT_H_ */
