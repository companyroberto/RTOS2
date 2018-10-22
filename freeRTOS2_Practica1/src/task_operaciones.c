/*============================================================================
 * Autor:		Nicolas Canadea + Roberto Compa�y
 * Fecha:		19/10/2018
 *===========================================================================*/


/*==================[inlcusiones]============================================*/

#include "task_operaciones.h"

// sAPI header
#include "sapi.h"

// Varias
#include <stdio.h>
#include <string.h>


/*==================[definiciones de datos internos]=========================*/


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

// Colas de operaciones
QueueHandle_t queMayusculizar;
QueueHandle_t queMinusculizar;
QueueHandle_t queTransmision;

#define	tamanio_queue_paquetes_pendientes	10								// Queue donde se almacenan los paquetes pendientes de procesar

enum protocolo					{ eProtocoloInicio_STX = 0x55, eProtocoloFin_ETX = 0xAA };
enum paquete_estados_e			{ eInicio, eOperacion, eLongDatos, eDatos, eFin };
enum paquete_operaciones_e		{ oMayusculizar, oMinusculizar, oStackDisponible, oHeapDisponible, oEstadoAplicacion };

typedef struct {
	uint8_t operacion;
	uint8_t	longDatos;
	QMPool * mem_pool;
	char *	block;
} queue_operaciones_t;


/*==================[declaraciones de funciones internas]====================*/


void recibir_fsm				( char c, BaseType_t * xHig );

int  hay_que_mayusculizar		( char c );
int  hay_que_minusculizar		( char c );


/*==================[definiciones de funciones externas]=====================*/


void
task_inicializar				( void )
{
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

	inicializarbuffer( &bufferCircular );
}

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
	if( leerBuffer( &bufferCircular, &byte ) ){
		uartTxWrite( UART_USB, byte );
	}
}

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

	//char diez1[10+1];char diez2[10+1];sprintf( diez1, "1234567890" );strncpy( diez2, diez1, 10);

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

			escribirBuffer( &bufferCircular, eProtocoloInicio_STX );
			escribirBuffer( &bufferCircular, queue_operaciones.operacion );
			escribirBuffer( &bufferCircular, queue_operaciones.longDatos );
			int i;
			for (i = 0; i < queue_operaciones.longDatos; i++)
				escribirBuffer( &bufferCircular, queue_operaciones.block[i] );

			escribirBuffer( &bufferCircular, eProtocoloFin_ETX );

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

	/*
	 * Comentario sobre dise�o:
	 * La funci�n 'stack_disponible' debe llamarse despues de utilizar la funci�n para que indique correctamente el valor.
	 * Mejor explicado: No alcanza con llamarla despu�s de crearla, sino que se debe haber ejecutado antes...
	 *
	 * Nota: Igual los n�meos no dan... si se descomenta la l�nea de 'task_minusculizar', la diferencia entre task_minusculizar
	 *       y task_mayusculizar son 6 bytes y deber�an ser m�nimo 20... (143 vs 137)
	 */

	//"La aplicaci�n transmitir� por la misma UART la memoria disponible en el stack de cada tarea que se cree..."
	stack_disponible( oMayusculizar );
	stack_disponible( oMinusculizar );

	// ---------- REPETIR POR SIEMPRE --------------------------
	while(TRUE) {
		vTaskDelay( 500 / portTICK_RATE_MS );

		gpioToggle( LED1 );					// Se�al de RTOS vivo...
	}
	//vTaskSuspend( pt_task_rtos_vivo );	// Prueba de punteros RTOS a tareas RTOS
}


/*==================[definiciones de funciones internas]=====================*/


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

/*==================[fin del archivo]========================================*/
