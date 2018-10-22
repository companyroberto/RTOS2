/*
 * modulos.h
 *
 *  Created on: 11/05/2012
 *      Author: alejandro
 */

#ifndef MODULOS_H_
#define MODULOS_H_

#include "FreeRTOS.h"
#include "queue.h"

typedef struct Modulo_t Modulo_t;
typedef struct Evento_t Evento_t;

typedef void (*fsm_ptr) (Evento_t *);

struct Modulo_t {
	fsm_ptr	manejadorEventos;
	int		prioridad;
	int		timeout_tick;
	int 		periodo;
};

struct Evento_t {
	Modulo_t *	receptor;
	int			signal;
	int			valor;
};

Modulo_t *	RegistrarModulo			( fsm_ptr manejadorEventos, int prioridad);
void			IniciarTodosLosModulos	( void );

extern xQueueHandle queEventosBaja;
extern xQueueHandle queEventosMedia;
extern xQueueHandle queEventosAlta;

extern Modulo_t * ModuloBroadcast;
extern Modulo_t * ModuloPulsadores;

#endif /* MODULOS_H_ */
