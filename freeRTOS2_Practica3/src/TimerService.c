/*
 * TimerService.c
 *
 *  Created on: 11/05/2012
 *      Author: alejandro
 */

#include "FreeRTOS.h"
#include "queue.h"
#include "sapi.h"

#include "FrameworkEventos.h"

enum {
	TIMER_EXPIRED  = 0,
	TIMER_DISABLED = -1
};

static bool_t timerHabilitado	( Modulo_t * m );
static bool_t timerCorriendo	( Modulo_t * m );

void 		vApplicationTickHook 		( void )
{
	int nro_modulo;
	Modulo_t * m;
	portBASE_TYPE cambiarCtx = pdFALSE;

	for( nro_modulo = 0; nro_modulo < ultimoModulo; ++nro_modulo ) {
		m = &modulos[nro_modulo];
		if(timerHabilitado(m)){
			if(--m->timeout_tick == TIMER_EXPIRED) {
				cambiarCtx = EncolarEventoFromISR(m, SIG_TIMEOUT, 0);
				m->periodo != TIMER_DISABLED ? timerRecargar(m) : timerDesarmar(m);
			}
		}
	}
	portEND_SWITCHING_ISR(cambiarCtx);
}

void timerArmarUnico			( Modulo_t * modulo, unsigned int timeout )
{
	portENTER_CRITICAL();
	modulo->timeout_tick = timeout;
	modulo->periodo 		= TIMER_DISABLED;
	portEXIT_CRITICAL();
	return;
}

void timerArmarRepetitivo	( Modulo_t * modulo, unsigned int timeout )
{
	portENTER_CRITICAL();
	modulo->periodo 		= timeout;
	modulo->timeout_tick = modulo->periodo;
	portEXIT_CRITICAL();
	return;
}

void timerRecargar			( Modulo_t * modulo )
{
	//portENTER_CRITICAL();
	modulo->timeout_tick	= modulo->periodo;
	//portEXIT_CRITICAL();
	return;
}

void timerDesarmar			( Modulo_t * modulo )
{
	portENTER_CRITICAL();
	modulo->timeout_tick = TIMER_DISABLED;
	portEXIT_CRITICAL();
	return;
}

//-----------------------------------------------------------------------------

static
bool_t timerHabilitado	( Modulo_t * m )
{
	return (m->timeout_tick != TIMER_DISABLED);
}

static
bool_t timerCorriendo	( Modulo_t * m )
{
	return (m->timeout_tick != TIMER_DISABLED && m->timeout_tick != TIMER_EXPIRED);
}
