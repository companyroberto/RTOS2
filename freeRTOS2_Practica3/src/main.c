/*============================================================================
 * Autor:		Nicolas Canadea + Roberto Compa�y
 * Fecha:		19/10/2018
 *===========================================================================*/


/*==================[NOTAS]====================================================

	Indicaciones por LED:

	LED3 (verde):	Sistema iniciado.				// en conflicto con requerimientos TP3
	LED2 (rojo):	Error en procesamiento.			// en conflicto con requerimientos TP3
	LED1 (amarillo)	RTOS activo. (si titila...)		// en conflicto con requerimientos TP3

=============================================================================*/


/*==================[inclusiones]============================================*/


// sAPI header
#include "sapi.h"

// Tareas RTOS
#include "task_operaciones.h"


#include "FrameworkEventos.h"
#include "pulsadores.h"
#include "leds.h"
#include "TiempoPulsacion.h"


/*==================[definiciones y macros]==================================*/


/*==================[definiciones de datos internos]=========================*/

Modulo_t * ModuloBroadcast;
Modulo_t * ModuloDriverPulsadores;
Modulo_t * ModuloLed;
Modulo_t * ModuloTiempoPulsacion;

/*==================[definiciones de datos externos]=========================*/

DEBUG_PRINT_ENABLE;

/*==================[función principal]======================================*/

// FUNCION PRINCIPAL, PUNTO DE ENTRADA AL PROGRAMA LUEGO DE ENCENDIDO O RESET.
int
main							( void )
{
	// ---------- CONFIGURACIONES ------------------------------

	// Inicializar y configurar la plataforma
	boardConfig();

	// Timer para task_Medir_Performance
	cyclesCounterConfig(EDU_CIAA_NXP_CLOCK_SPEED);

	// Configuracion UART
	debugPrintConfigUart( UART_USB, 115200 );

	// Gesti�n de interrupciones
	uartRxInterruptCallbackSet( UART_USB, recibir_UART );
	uartRxInterruptSet( UART_USB, true );

	//R2.5 : El envá­o de los datos se hará mediante la interrupción de â€œtransmisor vacá­oâ€�.
	uartTxInterruptCallbackSet( UART_USB, enviar_UART );
	uartTxInterruptSet( UART_USB, true );

	task_inicializar();

	heap_disponible();

	// Crear tarea en freeRTOS
	xTaskCreate(
			task_mayusculizar,                     	// Función de la tarea a ejecutar
			(const char *)"task_mayusculizar",		// Nombre de la tarea como String amigable para el usuario
			configMINIMAL_STACK_SIZE*2, 			// Cantidad de stack de la tarea
			0,                          			// Parámetros de tarea
			tskIDLE_PRIORITY+1+1,         			// Prioridad de la tarea
			&pt_task_mayusculizar					// Puntero a la tarea creada en el sistema
	);

	heap_disponible();

	// Crear tarea en freeRTOS
	xTaskCreate(
			task_minusculizar,                     	// Función de la tarea a ejecutar
			(const char *)"task_minusculizar",		// Nombre de la tarea como String amigable para el usuario
			configMINIMAL_STACK_SIZE*2, 			// Cantidad de stack de la tarea
			0,                          			// Parámetros de tarea
			tskIDLE_PRIORITY+1+1,         			// Prioridad de la tarea
			&pt_task_minusculizar					// Puntero a la tarea creada en el sistema
	);

	heap_disponible();

	// Crear tarea en freeRTOS
	xTaskCreate(
			task_transmision_UART, 		            // Función de la tarea a ejecutar
			(const char *)"task_transmision_UART",	// Nombre de la tarea como String amigable para el usuario
			configMINIMAL_STACK_SIZE*2, 			// Cantidad de stack de la tarea
			0,                          			// Parámetros de tarea
			tskIDLE_PRIORITY+1+1,         			// Prioridad de la tarea
			&pt_task_transmision_UART			    // Puntero a la tarea creada en el sistema
	);

	heap_disponible();

	// Crear tarea en freeRTOS
	xTaskCreate(
			task_medir_performance, 		        // Función de la tarea a ejecutar
			(const char *)"task_Medir_Performance",	// Nombre de la tarea como String amigable para el usuario
			configMINIMAL_STACK_SIZE*2, 			// Cantidad de stack de la tarea
			0,                          			// Parámetros de tarea
			tskIDLE_PRIORITY+1+1,         			// Prioridad de la tarea
			&pt_task_medir_merformance			    // Puntero a la tarea creada en el sistema
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

	// Manejador de eventos TP3 /////
	queEventosBaja = xQueueCreate(15, sizeof(Evento_t));

    // Creo la tarea de baja prioridad
    xTaskCreate(
    		taskDespacharEventos,				/* Pointer to the function that implements the task. */
			"Control",							/* Text name for the task.  This is to facilitate debugging only. */
			5 * configMINIMAL_STACK_SIZE,		/* Stack depth in words. */
			(void*) queEventosBaja,				/* Parametro de la tarea */
			tskIDLE_PRIORITY+1+1,				/* Prioridad */
			NULL );								/* Task handle. */							/* Task handle. */

    ModuloBroadcast			= RegistrarModulo(ManejadorEventosBroadcast, 			PRIORIDAD_BAJA);
    ModuloDriverPulsadores	= RegistrarModulo(DriverPulsadores, 					PRIORIDAD_BAJA);
    ModuloLed				= RegistrarModulo(DriverLeds, 							PRIORIDAD_BAJA);
    ModuloTiempoPulsacion	= RegistrarModulo(DriverTiempoPulsacion,				PRIORIDAD_BAJA);

    IniciarTodosLosModulos();
    //////////// Manejador de eventos


	estado_aplicacion("freeRTOS 2 - Practica 3 (v1.0)", FALSE, NULL);

	//R6: Al iniciar la aplicaci�n se reportar� por UART el heap disponible con la operaci�n
	//correspondiente, con el campo �Operaci�n� en su valor correspondiente.
	heap_disponible();

	// Sistema iniciado
	//gpioWrite( LED3, ON );	// en conflicto con requerimientos TP3

	// Timer para task_Medir_Performance
	cyclesCounterReset();

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
	//sprintf( c2, "uxHighWaterMark=%s!",c );	'!' caracter de fin
	//debugPrintString( c2 );
	//////////////////////////////////////////////
}
