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


static int estadoBroadcast = sBROADCAST_IDLE;
const char pulsadorApretado[] = "Pulsador Apretado:";
const char pulsadorLiberado[] = "Pulsador Liberado:";

void ManejadorEventosBroadcast (Evento_t * evn){
	char buf[100+1];
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

		case SIG_PULSADOR_APRETADO:
			buf[0]=0;
			//sprintf			(buf,"%s %d\r\n",pulsadorApretado, evn->valor);
			//UARTputs		(&consola, buf, UART_PUTS_INTERRUPCIONES);
			//ReenviarEvento	(ModuloPuertaControl, evn);
			break;

		case SIG_PULSADOR_LIBERADO:
			buf[0]=0;
			//sprintf			(buf,"%s %d\r\n",pulsadorLiberado, evn->valor);
			//UARTputs		(&consola, buf, UART_PUTS_INTERRUPCIONES);
			//ReenviarEvento	(ModuloPuertaControl, evn);
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
