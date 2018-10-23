/*
 * luz_sensor.c
 *
 *  Created on: 29/11/2011
 *      Author: alejandro
 */

#include "sapi.h"
#include "FrameworkEventos.h"
#include "pulsadores.h"

typedef enum EstadosPuertaSensorEnum
{
	sPULSADOR_IDLE = 0,
	sPULSADOR_ESPERANDO_PULSACION,
	sPULSADOR_CONFIRMANDO_PULSACION,
	sPULSADOR_ESPERANDO_LIBERACION,
	sPULSADOR_CONFIRMANDO_LIBERACION
} EstadosPuertaSensorEnum_t;

typedef struct PulsadorStruct
{
	gpioMap_t boton;
	int estadoFSM;
	int ticksFiltrados;
	int idPulsador;
} Pulsador;

void FSM_Pulsador ( Pulsador * pulsador, Evento_t * evn );

#define MAX_PULSADORES 4

Pulsador vectorPulsadores[MAX_PULSADORES];

static int idPulsador = 0;

static Modulo_t * mod;

void PulsadorInit ( Pulsador * p, gpioMap_t gpioInicializado )
{
	p->boton 			= gpioInicializado;
	p->estadoFSM 		= sPULSADOR_ESPERANDO_PULSACION;
	p->ticksFiltrados 	= 0;
	p->idPulsador 		= idPulsador;
	idPulsador++;
}

void DriverPulsadoresInit ( Modulo_t * pModulo )
{
	PulsadorInit( &vectorPulsadores[0], TEC1 );
	PulsadorInit( &vectorPulsadores[1], TEC2 );
	PulsadorInit( &vectorPulsadores[2], TEC3 );
	PulsadorInit( &vectorPulsadores[3], TEC4 );

	pModulo->periodo = 20;
}

void DriverPulsadores ( Evento_t *evn )
{
	int i;
	switch( evn->signal )
	{
		case SIG_MODULO_INICIAR:
			mod = (Modulo_t *)evn->receptor; // valor ??? ;
			DriverPulsadoresInit(mod);
			timerArmarRepetitivo(mod, mod->periodo);
			break;

		case SIG_TIMEOUT:
			for ( i = 0; i < idPulsador ; i++ )
			{
				FSM_Pulsador( &vectorPulsadores[i], evn );
			}
			break;

		default:	// Ignoro todas las otras seniales
			break;
	}
}

bool_t isActivo(gpioMap_t button){
	return !gpioRead( button );
}

void FSM_Pulsador ( Pulsador * pulsador, Evento_t * evn )
{
	Evento_t _evn;
	if( pulsador->ticksFiltrados > 0 )
	{
		pulsador->ticksFiltrados--;
		return;
	}

	switch ( pulsador->estadoFSM )
	{
		//-----------------------------------------------------------------------------
		case sPULSADOR_ESPERANDO_PULSACION:
			if( isActivo( pulsador->boton ) )
			{
				pulsador->estadoFSM 		= sPULSADOR_CONFIRMANDO_PULSACION;
				pulsador->ticksFiltrados 	= 0;
			}
			else
			{
				pulsador->ticksFiltrados 	= 5;
			}
			break;
			//-----------------------------------------------------------------------------
		case sPULSADOR_CONFIRMANDO_PULSACION:
			if( isActivo( pulsador->boton ) )
			{
				pulsador->estadoFSM 		= sPULSADOR_ESPERANDO_LIBERACION;
				pulsador->ticksFiltrados 	= 5;
				EncolarEvento( ModuloLed, SIG_BOTON_APRETADO, pulsador->idPulsador );
				EncolarEvento( ModuloTiempoPulsacion, SIG_BOTON_APRETADO, pulsador->idPulsador + 1 );
			}
			else
			{
				pulsador->estadoFSM 		= sPULSADOR_ESPERANDO_PULSACION;
				pulsador->ticksFiltrados 	= 0;
			}
			break;
			//-----------------------------------------------------------------------------
		case sPULSADOR_ESPERANDO_LIBERACION:
			if( !isActivo( pulsador->boton ) )
			{
				pulsador->estadoFSM 		= sPULSADOR_CONFIRMANDO_LIBERACION;
				pulsador->ticksFiltrados 	= 0;
			}
			else
			{
				pulsador->ticksFiltrados 	= 5;
			}
			break;
			//-----------------------------------------------------------------------------
		case sPULSADOR_CONFIRMANDO_LIBERACION:
			if( !isActivo( pulsador->boton ) )
			{
				//Liberacion confirmada, encolo un evento
				pulsador->estadoFSM 		= sPULSADOR_ESPERANDO_PULSACION;
				pulsador->ticksFiltrados 	= 5;
				EncolarEvento( ModuloLed, SIG_BOTON_LIBERADO, pulsador->idPulsador );
				EncolarEvento( ModuloTiempoPulsacion, SIG_BOTON_LIBERADO, pulsador->idPulsador + 1 );
				//gpioWrite( LED2, ON );
			}
			break;
			//-----------------------------------------------------------------------------
		default:
			// Se me fue de valor la variable de estado!
			// TODO: Reinicializar ESTE pulsador
			break;
	}
}


