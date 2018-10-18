/* Copyright 2017-2018, Eric Pernia + Nicolas Canadea + Roberto Compa�y
 * All rights reserved.
 *
 * This file is part of sAPI Library.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


/*==================[NOTAS]====================================================

	Indicaciones por LED:

	LED3 (verde):	Sistema iniciado.
	LED2 (rojo):	Error en procesamiento.
	LED1 (amarillo)	RTOS activo.

=============================================================================*/


/*==================[inclusiones]============================================*/

// Includes de FreeRTOS
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"

// sAPI header
#include "sapi.h"

// QM Pool
#include "qmpool.h"

// Varias
#include <string.h>
#include "stack_macros.h"

// BufferCircular utilizado para enviar por la UART
#include "bufferCircular.h"

/*==================[definiciones y macros]==================================*/

// Maximo RAM CIAA 139.264 B ( 136KB )

enum tamanio_pool				{ ePoolGrande   = 10,  ePoolMediano = 50,    ePoolChico   = 100 };
enum tamanio_bloque				{ eBloqueGrande = 255, eBloqueMediano = 128, eBloqueChico = 64  };

// Paquete máximo
static uint8_t memoria_para_pool_grande[ePoolGrande * eBloqueGrande];		// Espacio de almacenamiento para el Pool
QMPool mem_pool_grande;														// Estructura de control del Pool

// Paquete mediano
static uint8_t memoria_para_pool_mediano[ePoolMediano * eBloqueMediano];	// Espacio de almacenamiento para el Pool
QMPool mem_pool_mediano;													// Estructura de control del Pool

// Paquete chico
static uint8_t memoria_para_pool_chico[ePoolChico * eBloqueChico];			// Espacio de almacenamiento para el Pool
QMPool mem_pool_chico;														// Estructura de control del Pool

#define	tamanio_queue_paquetes_pendientes	10								// Queue donde se almacenan los paquetes pendientes de procesar

enum protocolo					{ eProtocoloInicio_STX = 0x55, eProtocoloFin_ETX = 0xAA };

/*==================[definiciones de datos internos]=========================*/

enum paquete_estados_e			{ eInicio, eOperacion, eLongDatos, eDatos, eFin };
enum paquete_operaciones_e		{ oMayusculizar, oMinusculizar, oStackDisponible, oHeapDisponible, oEstadoAplicacion };

typedef struct {
	uint8_t operacion;
	uint8_t	longDatos;
	QMPool * mem_pool;
	char *	block;
} queue_operaciones_t;


QueueHandle_t queMayusculizar;
QueueHandle_t queMinusculizar;
QueueHandle_t queTransmision;

bufferCircular_t buffer_UART;

/*==================[definiciones de datos externos]=========================*/

DEBUG_PRINT_ENABLE;

/*==================[declaraciones de funciones internas]====================*/

void recibir_UART				( void* noUsado );

void recibir_fsm				( char c, BaseType_t * xHig );

int  hay_que_mayusculizar		( char c );
int  hay_que_minusculizar		( char c );

void stack_disponible			( int operacion);
void heap_disponible			( );
void estado_aplicacion			( );

void enviar_UART				( void* noUsado );


/*==================[declaraciones de funciones externas]====================*/

TaskHandle_t pt_task_mayusculizar;
void task_mayusculizar( void* taskParmPtr );

TaskHandle_t pt_task_minusculizar;
void task_minusculizar( void* taskParmPtr );

TaskHandle_t pt_task_transmision_UART;
void task_transmision_UART( void* taskParmPtr );

TaskHandle_t pt_task_rtos_vivo;
void task_rtos_vivo( void* taskParmPtr );

/*==================[función principal]======================================*/

// FUNCION PRINCIPAL, PUNTO DE ENTRADA AL PROGRAMA LUEGO DE ENCENDIDO O RESET.
int
main							( void )
{
	// ---------- CONFIGURACIONES ------------------------------

	// Inicializar y configurar la plataforma
	boardConfig();

	// Configuracion UART
	debugPrintConfigUart( UART_USB, 115200 );
	inicializarbuffer( &buffer_UART );

	// Gestión de interrupciones
	uartRxInterruptCallbackSet( UART_USB, recibir_UART );
	uartRxInterruptSet( UART_USB, true );

	//R2.5 : El envá­o de los datos se hará mediante la interrupción de â€œtransmisor vacá­oâ€�.
	uartTxInterruptCallbackSet( UART_USB, enviar_UART );
	uartTxInterruptSet( UART_USB, true );

	// Colas para comunicar tareas segun operación
	queMayusculizar					= xQueueCreate( tamanio_queue_paquetes_pendientes, sizeof( queue_operaciones_t 	) );
	queMinusculizar					= xQueueCreate( tamanio_queue_paquetes_pendientes, sizeof( queue_operaciones_t 	) );
	queTransmision					= xQueueCreate( tamanio_queue_paquetes_pendientes, sizeof( queue_operaciones_t 	) );

	//Inicialización del Pool grande
	QMPool_init(&mem_pool_grande,
				memoria_para_pool_grande,
				sizeof(memoria_para_pool_grande),
				eBloqueGrande);

	//Inicialización del Pool mediano
	QMPool_init(&mem_pool_mediano,
				memoria_para_pool_mediano,
				sizeof(memoria_para_pool_mediano),
				eBloqueMediano);

	//Inicialización del Pool chico
	QMPool_init(&mem_pool_chico,
				memoria_para_pool_chico,
				sizeof(memoria_para_pool_chico),
				eBloqueChico);

	heap_disponible();

	// Crear tarea en freeRTOS
	xTaskCreate(
			task_mayusculizar,                     	// Función de la tarea a ejecutar
			(const char *)"task_mayusculizar",		// Nombre de la tarea como String amigable para el usuario
			configMINIMAL_STACK_SIZE*2, 			// Cantidad de stack de la tarea
			0,                          			// Parámetros de tarea
			tskIDLE_PRIORITY+1,         			// Prioridad de la tarea
			&pt_task_mayusculizar					// Puntero a la tarea creada en el sistema
	);

	heap_disponible();

	// Crear tarea en freeRTOS
	xTaskCreate(
			task_minusculizar,                     	// Función de la tarea a ejecutar
			(const char *)"task_minusculizar",		// Nombre de la tarea como String amigable para el usuario
			configMINIMAL_STACK_SIZE*2, 			// Cantidad de stack de la tarea
			0,                          			// Parámetros de tarea
			tskIDLE_PRIORITY+1,         			// Prioridad de la tarea
			&pt_task_minusculizar					// Puntero a la tarea creada en el sistema
	);

	heap_disponible();

	// Crear tarea en freeRTOS
	xTaskCreate(
			task_transmision_UART, 		            // Función de la tarea a ejecutar
			(const char *)"task_transmision_UART",	// Nombre de la tarea como String amigable para el usuario
			configMINIMAL_STACK_SIZE*2, 			// Cantidad de stack de la tarea
			0,                          			// Parámetros de tarea
			tskIDLE_PRIORITY+1,         			// Prioridad de la tarea
			&pt_task_transmision_UART			    // Puntero a la tarea creada en el sistema
	);

	// Crear tarea en freeRTOS
	xTaskCreate(
			task_rtos_vivo, 		            	// Función de la tarea a ejecutar
			(const char *)"task_rtos_vivo",			// Nombre de la tarea como String amigable para el usuario
			configMINIMAL_STACK_SIZE*2, 			// Cantidad de stack de la tarea
			0,                          			// Parámetros de tarea
			tskIDLE_PRIORITY+1,         			// Prioridad de la tarea
			&pt_task_rtos_vivo					    // Puntero a la tarea creada en el sistema
	);

	estado_aplicacion("freeRTOS 2 - Practica 1 (v1.7)");

	//R6: Al iniciar la aplicaci�n se reportar� por UART el heap disponible con la operaci�n
	//correspondiente, con el campo �Operaci�n� en su valor correspondiente.
	heap_disponible();

	//"La aplicaci�n transmitir� por la misma UART la memoria disponible en el stack de cada tarea que se cree..."
	stack_disponible( oMayusculizar );
	stack_disponible( oMinusculizar );

	// Sistema iniciado
	gpioWrite( LED3, ON );

	// Iniciar scheduler
	vTaskStartScheduler();

	// ---------- REPETIR POR SIEMPRE --------------------------
	while( TRUE ) {
		// Si cae en este while 1 significa que no pudo iniciar el scheduler
		gpioWrite( LED2, ON );
	}
	return 0;

	//////////////////////////// Enviar comentario
	//char c2[50];
	//memset( c2, 0, sizeof( c2 ) );
	//sprintf( c2, "uxHighWaterMark=%s!",c );
	//debugPrintString( c2 );
	//////////////////////////////////////////////
}

/*==================[definiciones de funciones internas]=====================*/

//R1.1 : La lectura de los datos se hará mediante la interrupción de â€œdato recibido por UARTâ€�.
void
recibir_UART					( void* noUsado )
{
	BaseType_t xHig;
	xHig = pdFALSE;

	char c = uartRxRead( UART_USB );
	recibir_fsm( c, &xHig );

	portYIELD_FROM_ISR(xHig);
}

// Sugerencias: Implementar el procesamiento de datos con una FSM.
void
recibir_fsm						( char c, BaseType_t * xHig )
{
	static int paquete_estados = eInicio;
	static int paquete_operaciones;
	static queue_operaciones_t queue_operaciones;
	static int byte_recibido;

	//R1 : La aplicación procesaran los paquetes de datos recibidos según el protocolo descrito anteriormente.
	switch ( paquete_estados )
	{
		case eInicio:
			//R1.2 : Se ignorarán los paquetes que no empiecen con STX.
			if ( c == eProtocoloInicio_STX )
				paquete_estados = eOperacion;
			else
				gpioWrite( LED2, ON );

			break;

		case eOperacion:
			if ( c >= 0 && c <= 4 ){
				paquete_operaciones = c;
				paquete_estados = eLongDatos;
			}else{
				paquete_estados = eInicio;
				gpioWrite( LED2, ON );
			}
			break;

		case eLongDatos:
			queue_operaciones.longDatos =  c;

			//R1.4 : Los paquetes válidos se copiarán a memoria dinámica asignada según el tamaá±o del paquete.
			//R3 : Se usará un algoritmo de asignación de memoria con tiempo de ejecución determinista.
			//R4 : Se usará un algoritmo de asignación de memoria que no produzca fragmentación del sistema.
			if ( queue_operaciones.longDatos 		< eBloqueChico )
				queue_operaciones.mem_pool = &mem_pool_chico;
			else if ( queue_operaciones.longDatos	< eBloqueMediano )
				queue_operaciones.mem_pool = &mem_pool_mediano;
			else
				queue_operaciones.mem_pool = &mem_pool_grande;

			queue_operaciones.block = QMPool_get( queue_operaciones.mem_pool, 0U, TRUE );		// Reserva la memoria

			byte_recibido = 0;
			paquete_estados = eDatos;
			break;

		case eDatos:
			queue_operaciones.block[byte_recibido] = c;

			byte_recibido++;
			if ( byte_recibido >= queue_operaciones.longDatos )
				paquete_estados = eFin;

			break;

		case eFin:
			//R1.3 : Se ignorarán los paquetes que no contengan ETX al final de los datos.
			if ( c != eProtocoloFin_ETX ){							// Si no viene el byte de FIN, descarto el paquete
				QMPool_put( queue_operaciones.mem_pool, queue_operaciones.block, TRUE );		// Libero el bloque de memoria
				gpioWrite( LED2, ON );
			}else{
				queue_operaciones.operacion=paquete_operaciones;

				switch (paquete_operaciones)
				{
					case oMayusculizar:
						//R1.5 : Se pondrá un puntero a paquete válido con operación 0 en la cola â€œqueMayusculizarâ€�.
						xQueueSendFromISR( queMayusculizar, &queue_operaciones, xHig );
						break;

					case oMinusculizar:
						//R1.7 : Se pondrá un puntero a paquete válido con operación 1 en la cola â€œqueMinusculizarâ€�.
						xQueueSendFromISR( queMinusculizar, &queue_operaciones, xHig );
						break;
					default:
						paquete_estados = eInicio;		// Si la operación es desconocida, reinicio la FSM
						break;
				}
			}

			paquete_estados = eInicio;
			break;

		default:
			paquete_estados = eInicio;
			gpioWrite( LED2, ON );
			break;
	}
}

int
hay_que_mayusculizar			( char byte )
{
	//R1.9 : Solo se modificarán los caracteres alfabéticos (a-z, A-Z).
	return byte >= 'a' && byte <= 'z';
}

int
hay_que_minusculizar			( char byte )
{
	//R1.9 : Solo se modificarán los caracteres alfabéticos (a-z, A-Z).
	return byte >= 'A' && byte <= 'Z';
}

void
stack_disponible				( int operacion)
{
	UBaseType_t uxHighWaterMark;

	if( operacion == oMayusculizar)
		uxHighWaterMark = uxTaskGetStackHighWaterMark( pt_task_mayusculizar );
	else
		uxHighWaterMark = uxTaskGetStackHighWaterMark( pt_task_minusculizar ); //80

	char c[50];
	memset( c, 0, sizeof( c ) );
	sprintf( c, "%d", uxHighWaterMark );

	queue_operaciones_t queue_operaciones;
	queue_operaciones.operacion = oStackDisponible;
	queue_operaciones.longDatos = strlen(c);
	queue_operaciones.mem_pool = &mem_pool_chico;

	queue_operaciones.block = QMPool_get( queue_operaciones.mem_pool, 0U, FALSE );		// Reserva la memoria
	strncpy( queue_operaciones.block, c, strlen(c));

	xQueueSend( queTransmision, &queue_operaciones, portMAX_DELAY );
}

void
heap_disponible					( )
{
	char c[50];
	memset( c, 0, sizeof( c ) );
	sprintf( c, "%d", xPortGetFreeHeapSize() );	//xPortGetMinimumEverFreeHeapSize()

	queue_operaciones_t queue_operaciones;
	queue_operaciones.operacion = oHeapDisponible;
	queue_operaciones.longDatos = strlen(c);
	queue_operaciones.mem_pool = &mem_pool_chico;
	queue_operaciones.block = QMPool_get( queue_operaciones.mem_pool, 0U, FALSE );		// Reserva la memoria
	strncpy( queue_operaciones.block, c, strlen(c) );

	xQueueSend( queTransmision, &queue_operaciones, portMAX_DELAY ); //portMAX_DELAY // 500 / portTICK_RATE_MS
}

void
estado_aplicacion				( char * msg )
{
	char c[255];
	memset( c, 0, sizeof( c ) );
	sprintf( c, "%s", msg );

	queue_operaciones_t queue_operaciones;
	queue_operaciones.operacion = oEstadoAplicacion;
	queue_operaciones.longDatos = strlen(c);

	if ( strlen(c) < eBloqueChico )
		queue_operaciones.mem_pool = &mem_pool_chico;
	else if ( strlen(c) < eBloqueMediano )
		queue_operaciones.mem_pool = &mem_pool_mediano;
	else
		queue_operaciones.mem_pool = &mem_pool_grande;

	queue_operaciones.block = QMPool_get( queue_operaciones.mem_pool, 0U, FALSE );		// Reserva la memoria
	strncpy( queue_operaciones.block, c, strlen(c));

	xQueueSend( queTransmision, &queue_operaciones, portMAX_DELAY );
}

//R2 : La aplicación devolverá por el mismo canal los paquete procesados según el protocolo descrito anteriormente.
void
enviar_UART						( void* noUsado )
{
	char byte;
	if( leerBuffer( &buffer_UART, &byte ) ){
		uartTxWrite( UART_USB, byte );
	}
}

/*==================[definiciones de funciones externas]=====================*/

void
task_mayusculizar				( void* taskParmPtr )
{
	// ---------- INICIALIZACION ------------------------------
	// ---------- REPETIR POR SIEMPRE --------------------------
	while(TRUE) {

		queue_operaciones_t queue_operaciones;
		xQueueReceive( queMayusculizar, &queue_operaciones, portMAX_DELAY );

		//R1.6 : Se convertirán a mayúsculas los paquetes recibidos en la cola â€œqueMayusculizarâ€�.
		int i;
		for (i = 0; i < queue_operaciones.longDatos; i++)
			if( hay_que_mayusculizar( queue_operaciones.block[i] ) )
			queue_operaciones.block[i] = toupper( queue_operaciones.block[i] );

		//R2.1 : Se pondrá un puntero a paquete mayusculizado en la cola â€œqueMayusculizadosâ€�.
		xQueueSend( queTransmision, &queue_operaciones, portMAX_DELAY );


	}
}

void
task_minusculizar				( void* taskParmPtr )
{
	// ---------- INICIALIZACION ------------------------------

	char diez1[10];
	char diez2[10];
	sprintf( diez1, "1234567890" );
	strncpy( diez2, diez1, 10);

	// ---------- REPETIR POR SIEMPRE --------------------------
	while(TRUE) {

		queue_operaciones_t queue_operaciones;
		xQueueReceive( queMinusculizar, &queue_operaciones, portMAX_DELAY );

		//R1.8 : Se convertirán a minúsculas los paquetes recibidos en la cola â€œqueMinusculizarâ€�.
		int i;
		for (i = 0; i < queue_operaciones.longDatos; i++)
			if( hay_que_minusculizar( queue_operaciones.block[i] ) )
				queue_operaciones.block[i] = tolower( queue_operaciones.block[i] );

		// R2.3 : Se pondrá un puntero a paquete minusculizado en la cola â€œqueMinusculizadosâ€�.
		xQueueSend( queTransmision, &queue_operaciones, portMAX_DELAY );
	}
}

void
task_transmision_UART			( void* taskParmPtr )
{
	// ---------- INICIALIZACION ------------------------------

	queue_operaciones_t queue_operaciones;
	BaseType_t q;

	// ---------- REPETIR POR SIEMPRE --------------------------
	while(TRUE) {
		//R2.2 : Los paquetes recibidos en la cola â€œqueMayusculizadosâ€� se transmitirán por la misma UART.
		//R2.4 : Los paquetes recibidos en la cola â€œqueMinusculizadosâ€� se transmitirán por la misma UART.
		q = xQueueReceive( queTransmision, &queue_operaciones, portMAX_DELAY );
		if ( q == pdTRUE ){	// pdFALSE

			escribirBuffer( &buffer_UART, eProtocoloInicio_STX );
			escribirBuffer( &buffer_UART, queue_operaciones.operacion );
			escribirBuffer( &buffer_UART, queue_operaciones.longDatos );
			int i;
			for (i = 0; i < queue_operaciones.longDatos; i++)
				escribirBuffer( &buffer_UART, queue_operaciones.block[i] );

			escribirBuffer( &buffer_UART, eProtocoloFin_ETX );

			//R2.6 : Al terminar de enviar un paquete se liberará la memoria dinámica asignada para el mismo.
			QMPool_put( queue_operaciones.mem_pool, queue_operaciones.block, FALSE );			// Libero el bloque de memoria

			//gpioWrite( LED2, ON );

			if( uartTxReady( UART_USB ) ){
				// La primera vez hay que llamar a la función para que empiece a escribir
				enviar_UART( 0 );
			}


			//R5: Luego de cada procesamiento de datos se reportar� por UART el m�nimo de stack
			//disponible en la tarea afectada, con el campo �Operaci�n� en su valor correspondiente.
			if( queue_operaciones.operacion == oMayusculizar || queue_operaciones.operacion == oMinusculizar )
				stack_disponible( queue_operaciones.operacion );
		}
	}
}

void
task_rtos_vivo					( void* taskParmPtr )
{
	// ---------- INICIALIZACION ------------------------------

	// ---------- REPETIR POR SIEMPRE --------------------------
	while(TRUE) {
		vTaskDelay( 500 / portTICK_RATE_MS );

		gpioToggle( LED1 );					// Señal de RTOS vivo... quitar en producción
	}
	//vTaskSuspend( pt_task_rtos_vivo );
}
/*==================[fin del archivo]========================================*/
