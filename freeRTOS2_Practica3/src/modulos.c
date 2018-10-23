/*
 * modulos.c
 *
 *  Created on: 11/05/2012
 *      Author: alejandro
 */

#include "FreeRTOS.h"
#include "queue.h"
#include "FrameworkEventos.h"

#define MAX_MODULOS 20
Modulo_t modulos[MAX_MODULOS];
int ultimoModulo = 0;

xQueueHandle queEventosBaja, queEventosMedia, queEventosAlta;

Modulo_t * RegistrarModulo (fsm_ptr manejadorEventos, int prioridad)
{
	if(ultimoModulo >= MAX_MODULOS)
		return (Modulo_t *) NULL;

	Modulo_t * pModulo = &modulos[ultimoModulo];
	pModulo->manejadorEventos	= manejadorEventos;
	pModulo->prioridad 			= prioridad;
	timerDesarmar( pModulo );
	ultimoModulo++;

	return pModulo;
}

void IniciarTodosLosModulos ( void )
{
	int modulo;
	Evento_t evn;
	for (modulo = 0; modulo < ultimoModulo; ++modulo) {
		EncolarEvento( &modulos[modulo], SIG_MODULO_INICIAR, 0 );
	}
}
