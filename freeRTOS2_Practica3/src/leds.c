/*============================================================================
 * Autor:		Nicolas Canadea + Roberto Compañy
 * Fecha:		19/10/2018
 *===========================================================================*/


#include "sapi.h"
#include "FrameworkEventos.h"
#include "leds.h"


typedef enum EstadosLeds
{
	sApagado = 0,
	sEncendido,
} EstadosLeds_t;

typedef struct LedStruct
{
	gpioMap_t Led;
	int estado;
	int idLed;
} Led;

#define MAX_LEDS 4

Led vectorLeds[MAX_LEDS];
static int nLeds = 0;

void LedInit ( Led * l, gpioMap_t gpioInicializado )
{
	l->Led	 	= gpioInicializado;
	l->estado 	= sApagado;
	l->idLed	= nLeds++;
}

void LedsInit ( Modulo_t * pModulo )
{
	LedInit( &vectorLeds[0], LEDB );	//LEDR,  LEDG,  LEDB,  LED1,  LED2,  LED3,
	LedInit( &vectorLeds[1], LED1 );	//LEDR,  LEDG,  LEDB,  LED1,  LED2,  LED3,
	LedInit( &vectorLeds[2], LED2 );	//LEDR,  LEDG,  LEDB,  LED1,  LED2,  LED3,
	LedInit( &vectorLeds[3], LED3 );	//LEDR,  LEDG,  LEDB,  LED1,  LED2,  LED3,
	//pModulo->periodo = 20;
}

static Modulo_t * mod;

void DriverLeds ( Evento_t *evn )
{
	int i;
	switch( evn->signal )
	{
		case SIG_MODULO_INICIAR:
			mod = (Modulo_t *) evn->receptor;
			LedsInit(mod);
			//timerArmarRepetitivo(mod, mod->periodo);
			timerArmarUnico(mod, mod->periodo);
			break;

		case SIG_BOTON_APRETADO:
			gpioWrite( LEDB + evn->valor , ON );
			break;

		case SIG_BOTON_LIBERADO:
			gpioWrite( LEDB + evn->valor , OFF );
			break;

		default:	// Ignoro todas las otras seniales
			break;
	}
}
