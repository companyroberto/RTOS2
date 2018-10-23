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
	SIG_BOTON_APRETADO,
	SIG_BOTON_LIBERADO
} Signal_t;

#endif /* SENIALES_H_ */
