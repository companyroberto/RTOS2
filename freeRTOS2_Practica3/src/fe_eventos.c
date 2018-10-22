/*
 * eventos.c
 *
 *  Created on: 15/06/2012
 *      Author: alejandro
 */

#include "FreeRTOS.h"
#include "queue.h"

#include "FrameworkEventos.h"

static Evento_t 		EsperarEvento			(xQueueHandle colaEventos);
static void				DespacharEvento		(Evento_t *evento);
static fsm_ptr			getManejadorEventos	(Evento_t * evento);
static xQueueHandle	getColaEventos			(int prioridad);

void taskDespacharEventos (void * paramColaEventos)
{
	/* Manejo de eventos de baja prioridad
	 * Esta siempre bloqueada salvo que tenga un evento que manejar.
	 * En cuanto hay un evento, lo despacho a la FSM correspondiente SIN BLOQUEAR */
	xQueueHandle colaEventos = (xQueueHandle) paramColaEventos;
	Evento_t evn;
	for(;;){
		evn = EsperarEvento(colaEventos);
		DespacharEvento(&evn);
	}
}

static
Evento_t EsperarEvento (xQueueHandle colaEventos)
{
	Evento_t evn;
	xQueueReceive(colaEventos, &evn, portMAX_DELAY);
	return evn;
}

static
void DespacharEvento(Evento_t *evento)
{
	fsm_ptr manejadorEventos = getManejadorEventos(evento);
	manejadorEventos(evento); //Al receptor del evento le paso el evento
}

static
fsm_ptr getManejadorEventos (Evento_t * evento)
{
	return (evento->receptor)->manejadorEventos;
}

//-----------------------------------------------------------------------------

void EncolarEvento (Modulo_t * receptor, Signal_t senial, int valor)
{
	xQueueHandle colaEventos = getColaEventos(receptor->prioridad);
	Evento_t evn;
	evn.receptor = receptor;
	evn.signal = senial;
	evn.valor = valor;

	xQueueSend(colaEventos, &evn, 0);
	//TODO: Manejar error de cola de eventos llena
	return;
}

portBASE_TYPE EncolarEventoFromISR (Modulo_t * receptor, Signal_t senial, int valor)
{
	xQueueHandle colaEventos = getColaEventos(receptor->prioridad);
	portBASE_TYPE cambiarCtx = pdFALSE;
	Evento_t evn;
	evn.receptor = receptor;
	evn.signal = senial;
	evn.valor = valor;

	xQueueSendFromISR(colaEventos, &evn, &cambiarCtx);
	return cambiarCtx;
}

static
xQueueHandle getColaEventos(int prioridad)
{
	xQueueHandle colaEventos;
	switch (prioridad)
	{
		case PRIORIDAD_BAJA:  colaEventos = queEventosBaja;  break;
		case PRIORIDAD_MEDIA: colaEventos = queEventosMedia; break;
		case PRIORIDAD_ALTA:  colaEventos = queEventosAlta;  break;
		default:
			colaEventos = 0; //Nos vamos derechito a un cuelgue
			break;
	}
	return colaEventos;
}
