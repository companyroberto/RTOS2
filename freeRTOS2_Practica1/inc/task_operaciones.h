/*============================================================================
 * Autor:		Nicolas Canadea + Roberto Compañy
 * Fecha:		19/10/2018
 *===========================================================================*/


#ifndef _TASK_OPERACIONES_H_
#define _TASK_OPERACIONES_H_

// Includes de FreeRTOS
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"

// QM Pool
#include "qmpool.h"

// BufferCircular utilizado para enviar por la UART
#include "bufferCircular.h"


/*==================[definiciones de datos publicos]=========================*/


/*==================[declaración de funciones externas]=========================*/

void task_inicializar			( void );

void recibir_UART				( void* noUsado );

void stack_disponible			( int operacion);
void heap_disponible			( );
void estado_aplicacion			( );

void enviar_UART				( void* noUsado );


TaskHandle_t pt_task_mayusculizar;
void task_mayusculizar( void* taskParmPtr );

TaskHandle_t pt_task_minusculizar;
void task_minusculizar( void* taskParmPtr );

TaskHandle_t pt_task_transmision_UART;
void task_transmision_UART( void* taskParmPtr );

TaskHandle_t pt_task_rtos_vivo;
void task_rtos_vivo( void* taskParmPtr );

#endif
