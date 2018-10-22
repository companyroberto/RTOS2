/*
 * seniales.h
 *
 *  Created on: 15/06/2012
 *      Author: alejandro
 */

#ifndef SENIALES_H_
#define SENIALES_H_

//Aqui se deben listar las seniales que genera la aplicacion
typedef enum {
	SIG_MODULO_INICIAR = 0,
	SIG_TIMEOUT,
	SIG_PULSADOR_APRETADO,
	SIG_PULSADOR_LIBERADO,
	SIG_ULTIMO_VALOR = SIG_PULSADOR_LIBERADO
} Signal_t;

#endif /* SENIALES_H_ */
