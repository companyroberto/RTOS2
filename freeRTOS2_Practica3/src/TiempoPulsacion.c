/*============================================================================
 * Autor:		Nicolas Canadea + Roberto Compañy
 * Fecha:		19/10/2018
 *===========================================================================*/


#include "sapi.h"
#include "FrameworkEventos.h"
#include "TiempoPulsacion.h"

#include "task_operaciones.h"

typedef struct PulsadorStruct
{
	int idPulsador;
	int tiempoInicial;
	int tiempoFinal;
} Pulsador;

#define MAX_PULSADORES 4

Pulsador vectorTpoPulsadores[MAX_PULSADORES];

void TpoPulsadorInit ( Pulsador * p, int _idPulsador )
{
	p->idPulsador	 	= _idPulsador;
	p->tiempoInicial 	= 0;
	p->tiempoFinal		= 0;
}

void TpoPulsadoresInit ( Modulo_t * pModulo )
{
	TpoPulsadorInit( &vectorTpoPulsadores[0], 1 );
	TpoPulsadorInit( &vectorTpoPulsadores[1], 2 );
	TpoPulsadorInit( &vectorTpoPulsadores[2], 3 );
	TpoPulsadorInit( &vectorTpoPulsadores[3], 4 );
}

static Modulo_t * mod;

void DriverTiempoPulsacion ( Evento_t *evn )
{
	switch( evn->signal )
	{
		case SIG_MODULO_INICIAR:
			mod = (Modulo_t *) evn->receptor;
			TpoPulsadoresInit(mod);
			timerArmarUnico(mod, mod->periodo);
			break;

		case SIG_BOTON_APRETADO:
			vectorTpoPulsadores[ evn->valor ].tiempoInicial = xTaskGetTickCount();
			break;

		case SIG_BOTON_LIBERADO:
			vectorTpoPulsadores[ evn->valor ].tiempoFinal = xTaskGetTickCount();
			TickType_t diferenciaTick = vectorTpoPulsadores[ evn->valor ].tiempoFinal - vectorTpoPulsadores[ evn->valor ].tiempoInicial;

			tiempo_boton_oprimido( diferenciaTick, evn->valor + 1 );
			break;

		default:	// Ignoro todas las otras seniales
			break;
	}
}
