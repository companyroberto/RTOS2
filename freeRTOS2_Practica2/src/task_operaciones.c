/*============================================================================
 * Autor:		Nicolas Canadea + Roberto Compa�y
 * Fecha:		19/10/2018
 *===========================================================================*/


/*==================[inlcusiones]============================================*/

#include "task_operaciones.h"

// sAPI header
#include "sapi.h"

// QM Pool
#include "qmpool.h"

// BufferCircular utilizado para enviar por la UART
#include "bufferCircular.h"

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
QueueHandle_t quePerformance;
QueueHandle_t queMedirPerformance;

#define	tamanio_queue_paquetes_pendientes	10								// Queue donde se almacenan los paquetes pendientes de procesar

enum protocolo					{ eProtocoloInicio_STX = 0x55, eProtocoloFin_ETX = 0xAA };
enum paquete_estados_e			{ eInicio, eOperacion, eLongDatos, eDatos, eFin };
enum paquete_operaciones_e		{ oMayusculizar, oMinusculizar, oStackDisponible, oHeapDisponible, oEstadoAplicacion, oPerformance };

// Medir performance
typedef enum estado_medicion    { eT_LL, eT_R, eT_I, eT_F, eT_S, eT_T } estado_mp;

static uint32_t  id_de_paquete = 0;

typedef struct {
	estado_mp estado_Token;          				//Estado para la fsm_medir_performance();

	uint32_t  id_de_paquete;         				//id_de_paquete=0 ---> id_de_paquete++
	char * payload;					 				//apuntar� al paquete de datos a procesar.
    uint32_t  tiempo_de_llegada;     				//De la llegada del paquete.
    uint32_t  tiempo_de_recepcion;   				//Del fin del paquete recibido.
    uint32_t  tiempo_de_inicio;      				//Inicio de la operación (Mayusculizar).
    uint32_t  tiempo_de_fin;         				//Fin de la operación (Mayusculizar):
    uint32_t  tiempo_de_salida;      				//Tiempo de inicio de transmisión [STX].
    uint32_t  tiempo_de_transmision; 				//Tiempo de fin de transmisión [ETX].
    uint16_t  largo_del_paquete;     				//Nº de bytes (Datos)+ Header=4 bytes.
    uint16_t  memoria_alojada;       				//Tamaño del pool.
    void (*ptr_completion_handler)( void *T, BaseType_t * xHig );		//Puntero a función.
} Token_t;
////////////////////

typedef struct {
	uint8_t operacion;
	uint8_t	longDatos;
	QMPool * mem_pool;
	char *	block;
	Token_t * Token;
} queue_operaciones_t;


/*==================[declaraciones de funciones internas]====================*/


void recibir_fsm				( char c, BaseType_t * xHig );

int  hay_que_mayusculizar		( char c );
int  hay_que_minusculizar		( char c );

void callback_medir_performance ( void *T, BaseType_t * xHig );

void fsm_medir_performance      ( Token_t * ptT );


/*==================[definiciones de funciones externas]=====================*/


void
task_inicializar				( void )
{
	// Colas para comunicar tareas segun operación
	queMayusculizar					= xQueueCreate( tamanio_queue_paquetes_pendientes, sizeof( queue_operaciones_t 	) );
	queMinusculizar					= xQueueCreate( tamanio_queue_paquetes_pendientes, sizeof( queue_operaciones_t 	) );
	queTransmision					= xQueueCreate( tamanio_queue_paquetes_pendientes, sizeof( queue_operaciones_t 	) );
	quePerformance					= xQueueCreate( tamanio_queue_paquetes_pendientes, sizeof( queue_operaciones_t 	) );
	queMedirPerformance				= xQueueCreate( tamanio_queue_paquetes_pendientes, sizeof( Token_t				) );

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
estado_aplicacion				( char * msg, uint8_t interrupcion, BaseType_t * xHig )
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


	queue_operaciones.block = QMPool_get( queue_operaciones.mem_pool, 0U, interrupcion );		// Reserva la memoria
	strncpy( queue_operaciones.block, c, strlen(c));

	if( interrupcion ){
		xQueueSendFromISR( queTransmision, &queue_operaciones, xHig );
	}else{
		xQueueSend( queTransmision, &queue_operaciones, portMAX_DELAY  );
	}
}

//R2 : La aplicación devolverá por el mismo canal los paquete procesados según el protocolo descrito anteriormente.
void
enviar_UART						( void* noUsado )
{
	static int inicio_detectado = 0;
	static int operacion_performance = 0;

	char byte;
	if( leerBuffer( &bufferCircular, &byte ) ){

		uartTxWrite( UART_USB, byte );

		if( operacion_performance == 1 && byte == eProtocoloFin_ETX ){
			BaseType_t xHig;
			xHig = pdFALSE;

			Token_t Token;
			xQueueReceiveFromISR( queMedirPerformance, &Token, &xHig );

			// Despues de trasmitir
			fsm_medir_performance( &Token );

			Token.ptr_completion_handler( &Token, &xHig );

			portYIELD_FROM_ISR(xHig);
			operacion_performance = 0;
		}

		if( inicio_detectado == 1 ){
			if( byte == oPerformance ){
				operacion_performance = 1;
			}
			inicio_detectado = 0;
		}

		if( byte == eProtocoloInicio_STX ){
			inicio_detectado = 1;
		}
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
		if ( q == pdTRUE ){

			escribirBuffer( &bufferCircular, eProtocoloInicio_STX );
			escribirBuffer( &bufferCircular, queue_operaciones.operacion );
			escribirBuffer( &bufferCircular, queue_operaciones.longDatos );
			int i;
			for (i = 0; i < queue_operaciones.longDatos; i++)
				escribirBuffer( &bufferCircular, queue_operaciones.block[i] );

			escribirBuffer( &bufferCircular, eProtocoloFin_ETX );

			//R2.6 : Al terminar de enviar un paquete se liberará la memoria dinámica asignada para el mismo.
			QMPool_put( queue_operaciones.mem_pool, queue_operaciones.block, FALSE );			// Libero el bloque de memoria


			if( queue_operaciones.operacion == oPerformance ){
				Token_t Token;
				xQueueReceive( queMedirPerformance, &Token, portMAX_DELAY );

				// Antes de empezar a trasmitir
				fsm_medir_performance( &Token );

				xQueueSend( queMedirPerformance, &Token, portMAX_DELAY );
			}


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
task_medir_performance			( void* taskParmPtr )
{
	// ---------- INICIALIZACION ------------------------------
	// ---------- REPETIR POR SIEMPRE --------------------------
	while(TRUE) {

		queue_operaciones_t queue_operaciones;
		xQueueReceive( quePerformance, &queue_operaciones, portMAX_DELAY );

		Token_t Token;
		xQueueReceive( queMedirPerformance, &Token, portMAX_DELAY );

		// Antes de mayusculizar
		fsm_medir_performance( &Token );

		//R1.6 : Se convertirán a mayúsculas los paquetes recibidos en la cola â€œqueMayusculizarâ€�.
		int i;
		for (i = 0; i < queue_operaciones.longDatos; i++)
			if( hay_que_mayusculizar( queue_operaciones.block[i] ) )
			queue_operaciones.block[i] = toupper( queue_operaciones.block[i] );

		// Despues de mayusculizar
		fsm_medir_performance( &Token );

		xQueueSend( queMedirPerformance, &Token, portMAX_DELAY );

		//R2.1 : Se pondrá un puntero a paquete mayusculizado en la cola â€œqueMayusculizadosâ€�.
		xQueueSend( queTransmision, &queue_operaciones, portMAX_DELAY );
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

	// Inicializar medir performance
	static Token_t Token;

	//R1 : La aplicación procesaran los paquetes de datos recibidos según el protocolo descrito anteriormente.
	switch ( paquete_estados )
	{
		case eInicio:
			//R1.2 : Se ignorarán los paquetes que no empiecen con STX.
			if ( c == eProtocoloInicio_STX ){
				paquete_estados = eOperacion;

				Token.estado_Token = eT_LL;
				fsm_medir_performance( &Token );
			}
			else
				gpioWrite( LED2, ON );

			break;

		case eOperacion:
			if ( c >= 0 && c <= 5 ){
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
			uint16_t memoria_alojada;

			if ( queue_operaciones.longDatos 		< eBloqueChico ){
				queue_operaciones.mem_pool = &mem_pool_chico;
				memoria_alojada = sizeof(memoria_para_pool_chico);
			}
			else if ( queue_operaciones.longDatos	< eBloqueMediano ){
				queue_operaciones.mem_pool = &mem_pool_mediano;
				memoria_alojada = sizeof(memoria_para_pool_mediano);
			}
			else{
				queue_operaciones.mem_pool = &mem_pool_grande;
				memoria_alojada = sizeof(memoria_para_pool_grande);
			}

			queue_operaciones.block = QMPool_get( queue_operaciones.mem_pool, 0U, TRUE );		// Reserva la memoria

			byte_recibido = 0;
			paquete_estados = eDatos;

			if( paquete_operaciones == oPerformance ){
				Token.memoria_alojada = memoria_alojada;
				Token.largo_del_paquete = queue_operaciones.longDatos;
				Token.payload = queue_operaciones.block;
			}

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
				if( paquete_operaciones == oPerformance ){
					fsm_medir_performance( &Token );
				}

				queue_operaciones.operacion = paquete_operaciones;

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

					case oPerformance:
						/*
						 * Comentario sobre dise�o:
						 * Se mantiene la estructura 'queue_operaciones' con el atributo token asociado segun requerimientos.						 *
						 */
						xQueueSendFromISR( quePerformance, &queue_operaciones, xHig );
						// R2: Los punteros a token asociados a mediciones de performance se recoger�n de la cola �queMedirPerformance�.
						xQueueSendFromISR( queMedirPerformance, &Token, xHig );
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
callback_medir_performance		( void *p, BaseType_t * xHig )
{
	Token_t * T = (Token_t*) p;

 	char s1[200];
	char s2[20];
	memset(s1,0,sizeof(s1));
	memset(s2,0,sizeof(s2));
	sprintf( s1, " ID:%d",T->id_de_paquete);
	sprintf( s2, " eT_LL:%d us-",T->tiempo_de_llegada);
    strcat(s1,s2);
    sprintf( s2, " eT_R:%d us-",T->tiempo_de_recepcion);
    strcat(s1,s2);
    sprintf( s2, " eT_I:%d us-",T->tiempo_de_inicio);
    strcat(s1,s2);
    sprintf( s2, " eT_F:%d us-",T->tiempo_de_fin);
    strcat(s1,s2);
    sprintf( s2, " eT_S:%d us-",T->tiempo_de_salida);
    strcat(s1,s2);
    sprintf( s2, " eT_T:%d us-",T->tiempo_de_transmision);
    strcat(s1,s2);
    sprintf( s2, " l_p:%d bytes-",T->largo_del_paquete);
    strcat(s1,s2);
    sprintf( s2, " t_p:%d bytes-",T->memoria_alojada);
    strcat(s1,s2);

    estado_aplicacion( s1, TRUE, xHig );
}

void
fsm_medir_performance 			( Token_t * ptr )
{

	switch( ptr->estado_Token )
	{
	case eT_LL:
		ptr->tiempo_de_llegada = cyclesCounterToUs(cyclesCounterRead());
		ptr->estado_Token = eT_R;
		break;

	case eT_R:
		// Se inicializan campos del token
		ptr->id_de_paquete = id_de_paquete++;
		ptr->ptr_completion_handler = callback_medir_performance;

		ptr->tiempo_de_recepcion = cyclesCounterToUs(cyclesCounterRead());
		ptr->estado_Token = eT_I;
		break;

	case eT_I:
		ptr->tiempo_de_inicio = cyclesCounterToUs(cyclesCounterRead());
		ptr->estado_Token=eT_F;
		break;

	case eT_F:
		ptr->tiempo_de_fin = cyclesCounterToUs(cyclesCounterRead());
		ptr->estado_Token=eT_S;
		break;

	case eT_S:
		ptr->tiempo_de_salida = cyclesCounterToUs(cyclesCounterRead());
		ptr->estado_Token=eT_T;
		break;

	case eT_T:
		ptr->tiempo_de_transmision = cyclesCounterToUs(cyclesCounterRead());
		break;

	default:
		gpioWrite( LED2, ON );	// Si la fsm no tiene un estado inicial, ya no se comporta como se espera

		break;
	}

}


/*==================[fin del archivo]========================================*/
