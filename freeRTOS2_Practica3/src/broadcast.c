/*
 * broadcast.c
 *
 *  Created on: 29/11/2011
 *      Author: alejandro
 */

#include "FrameworkEventos.h"

typedef enum estadosBroadcastEnum {
	sBROADCAST_IDLE		 = 0,
	sBROADCAST_NORMAL
} EstadosBroadcast_t;


static int estadoBroadcast	  = sBROADCAST_IDLE;

void ManejadorEventosBroadcast (Evento_t * evn){

	switch(estadoBroadcast){
//-----------------------------------------------------------------------------
	case sBROADCAST_IDLE:
		switch(evn->signal){
		case SIG_MODULO_INICIAR:
			estadoBroadcast = sBROADCAST_NORMAL;
			break;

		default:
			break;
		}
		break;
//-----------------------------------------------------------------------------
	case sBROADCAST_NORMAL:
		switch(evn->signal){

			break;

		default:
			break;
		}
		break;
//-----------------------------------------------------------------------------
	default:
		break;
	}
}
