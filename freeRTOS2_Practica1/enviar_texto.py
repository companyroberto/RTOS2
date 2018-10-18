#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
C:\CIAA\cese-edu-ciaa-template-master\proyectosTitO
python enviar_texto.py prueba.txt COM4 0

(04hola)

Created on Wed Sep 19 09:25:55 2018
@author: alejandro
"""

import signal
import sys
import argparse
import array
import time
try:
    import serial
except:
    print('Falta instalar pyserial')
    print('Pruebe con pip3 install pyserial.')

#===============================================================================

def signal_handler(sig, frame):
    print('Se pulsó Ctrl+C!, saliendo del programa!')
    try:
        puerto.close()
    except:
        pass
    else:
        print('Se cerró el puerto OK.')
    sys.exit(0)    

def cartel_de_envio(args):
    if args['operacion'] == 0:
        str_oper = 'mayusculizar'
    elif args['operacion'] == 1:
        str_oper = 'minusculizar'
    else:
        str_oper = 'medir perfomance'

    print('--------------------------------------------------------------------------------')
    print('--- Enviando {} por {} para {}'.format(
        args['nombre_archivo'],
        args['puerto'],
        str_oper))
    print('--------------------------------------------------------------------------------')

def cartel_de_recepcion():
    print('--------------------------------------------------------------------------------')
    print('--- Respuestas recibidas -------------------------------------------------------')
    print('--------------------------------------------------------------------------------') 

def enviar_archivo(archivo, puerto, operacion):
    i = 0
    archivo.seek(0)
    for line in archivo:
        if line not in ['\n', '\r', '\r\n']:
            buf = bytearray()
            buf.append(STX)
            buf.append(operacion)
            buf.append(len(line))
            buf.extend(bytes(line, encoding='UTF-8'))
            buf.append(ETX)
            puerto.write(buf)
            buf = buf.replace(b'\n', b'')
            i += 1
            print('Linea {}: 0x{:X}-{:02}-{:02}-{}-0x{:X}'.format(str(i).rjust(2), buf[0], buf[1], buf[2], str(buf[3:-1],encoding='latin-1'), buf[-1]))
    return i

def mostrar_respuestas(puerto, n_lineas_esperadas):
    eco = bytes()
    j = 0
    while True:
        char = puerto.read()
        eco += char
        if char == b'\x21': #!fin cadena
            print('Comentarios: {}'.format(str(eco,encoding='latin-1')))
            eco = bytes()	#comentarios
        if char == b'\xaa':
            eco = eco.replace(b'\n', b'')
            j += 1
            print('Linea {}: 0x{:X}-{:02}-{:02}-{}-0x{:X}'.format(str(j).rjust(2), eco[0], eco[1], eco[2], str(eco[3:-1],encoding='latin-1'), eco[-1]))
            if j < n_lineas_esperadas:
                eco = bytes() #nuevo
            else:
                return

def obtener_argumentos_cli():
    parser = argparse.ArgumentParser(
            prog='enviar_texto.py',
            usage='python3 enviar-texto.py <nombre_archivo> <nombre_puerto> <operacion>',    
            description='Transmite un <archivo> por <puerto> a 115200bps.')    
        
    parser.add_argument(
            dest='nombre_archivo',
            action='store',
            type=str,
            help='El archivo a abrir.')

    parser.add_argument(
            dest='puerto',
            action='store',
            type=str,
            help='El puerto serie a abrir.')

    parser.add_argument(
            dest='operacion',
            action='store',
            type=int,
            choices=[0, 1, 5],
            help='La operacion 0 (Mayusculizar), 1 (minusculizar), 5(medir perfomance)')

    return vars(parser.parse_args())

#===============================================================================
    
signal.signal(signal.SIGINT, signal_handler)

STX = 0x55
ETX = 0xAA

args = obtener_argumentos_cli()

try:
    puerto = serial.Serial(args['puerto'], 115200)
except:
    print('No se puede abrir el puerto')
    exit()

mostrar_respuestas(puerto, 7)						# Mensajes iniciales

with open( (args['nombre_archivo']), 'rt') as archivo:
    for j in range(1):
        cartel_de_envio(args)
        lineas_enviadas = enviar_archivo(archivo, puerto, args['operacion'])
        cartel_de_recepcion()
        mostrar_respuestas(puerto, lineas_enviadas * 2)	# Respuesta + STACK

#===============================================================================

