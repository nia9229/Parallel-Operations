/**
 * Addition, subtraction and XOR array values  | Parallel Programming
 * Nicolas Amigo Sanudo
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <mutex>
#include <math.h>
#include <condition_variable>
#include <sys/time.h>
#include <atomic>

//#define DEBUG 
//#define FEATURE_LOGGER 
//#define FEATURE_OPTIMIZE

// mutexes
std::mutex g_m;
std::mutex g_m2;
std::mutex g_m3;

// variables condicionales
std::condition_variable g_c;
std::condition_variable g_c2;
bool g_ready = false;
bool g_ready2 = true;

// si es true el array restante ya ha sido operado
bool g_restante_operado = false;

// definicion de las variable atomica para optimizacion
#if defined(FEATURE_OPTIMIZE)
	std::atomic<bool> g_atomic_restante(false);
#endif

// modos de funcionamiento
enum modo {SINGLE, MULTI};

// operaciones a efectuar
enum operacion {SUM, SUB, XOR};

// estructura con los datos a operar
struct datos {
	double *array;
	double *array_restante;
};

// estructura con las variables del logger
struct datos_logger {
	double resultado_parcial;
	double *resultados_logger;
	double resultado_total;
	int posicion_thread;
	int hilos_trabajados;
	std::condition_variable c;
	bool ready;
};

struct opciones {
	int tamanho_array; 
	int tamanho_array_restante;
	operacion op;
	modo mod; 
	int numero_threads;
};

// prototipo de la funcion del logger
void funcion_logger(datos_logger *elementos_logger, opciones *parametros);

// prototipo de la funcion de los threads
void funcion(opciones *parametros, datos *elementos, datos_logger *elementos_logger, double *array_resultados, int posicion_thread);

/* Usage: ./p1 <array_length> <operation> [--multi-thread<num threads>]*/

int main(int argc, char *argv[]) {
	struct datos elementos;
	struct opciones parametros;
	
	
	struct datos_logger elementos_logger;
	elementos_logger.ready = false;
	elementos_logger.hilos_trabajados = 0;
	
	// array con los resultados de cada thread
	double *array_resultados;
	
	double resultado_total = 0;
	double tiempo_total;
	
	// salida del main
	int estado = 0;
	
	struct timeval comienzo, fin;
	
	// inicializacion de los parametos de la estructura
	if (argc == 3) {
		parametros.mod = SINGLE; 
		parametros.tamanho_array = atoi(argv[1]); 
		parametros.numero_threads = 1;
		
	} else if (argc == 5) {
		if (strcmp(argv[3], "--multi-thread") == 0 && (atoi(argv[4]) >= 1 && atoi(argv[4]) <= 12)) {
			parametros.mod = MULTI; 
			parametros.tamanho_array = atoi(argv[1]); 
			parametros.numero_threads = atoi(argv[4]);
			#if defined(DEBUG) 
				printf("EL numero de threads es %d\n", parametros.numero_threads);
			#endif
		} else {
			printf("La operacion no es valida\n");
		return 0;
		}
	} else {
		printf("El numero de argumentos no es valido\n");
		return 0;
	}
	
	// obtencion de la operacion a efectuar
	if (strcmp(argv[2], "sum") == 0) {
		parametros.op = SUM;
	} else if (strcmp(argv[2], "sub") == 0) {
		parametros.op = SUB;
	} else if (strcmp(argv[2], "xor") == 0) {
		parametros.op = XOR;
	} else {
		printf("La operacion no es valida\n");
		return 0;
	}
	
	#if defined(DEBUG)
		printf("La operacion a efectuar es %s\n\n",argv[2]);
	#endif
	
	// asignacion de memoria del array de los resultados de cada hilo
	array_resultados = (double*) malloc(parametros.numero_threads * sizeof(double));
	
	// inicializacion del array
	elementos.array = (double*) malloc(parametros.tamanho_array * sizeof(double));
	for (int i = 0; i < parametros.tamanho_array; i++) {
		elementos.array[i] = (double)i;
	}
	
	// inicializacion del array de elementos restantes si no es exacta la asignacion
	parametros.tamanho_array_restante = parametros.tamanho_array % parametros.numero_threads;
	int inicio = parametros.tamanho_array - parametros.tamanho_array_restante;
	if (parametros.numero_threads < parametros.tamanho_array && parametros.tamanho_array_restante != 0) {
		elementos.array_restante = (double*) malloc(parametros.tamanho_array_restante * sizeof(double));
		for (int i = 0; i < parametros.tamanho_array_restante; i++) {
			elementos.array_restante[i] = elementos.array[i+inicio];
		}
	}
	
	#if defined(FEATURE_LOGGER)
		elementos_logger.resultados_logger = (double*) malloc(parametros.numero_threads * sizeof(double));
		std::thread logger(funcion_logger,&elementos_logger,&parametros);
	#endif
	
	// comienzo del tiempo
	gettimeofday(&comienzo, NULL);
	std::thread threads[parametros.numero_threads]; // creacion de los hilos
	for (int i = 0; i < parametros.numero_threads; i++) {
		threads[i] = std::thread(funcion,&parametros,&elementos,&elementos_logger,array_resultados,i);
	}
	
	// sincronizacion de los hilos
	for (int i = 0; i < parametros.numero_threads; i++) {
		threads[i].join();
	}
	gettimeofday(&fin, NULL);
	// final del tiempo
	
	// espera del main al logger
	#if defined(FEATURE_LOGGER)
		std::unique_lock<std::mutex> ulk(g_m3);
		elementos_logger.c.wait(ulk, [&elementos_logger]{return elementos_logger.ready;});
		ulk.unlock();
		logger.join();
		if (resultado_total != elementos_logger.resultado_total) {
			estado = 1;
		}
		printf("\nEl resultado del logger es %f\n", elementos_logger.resultado_total);
	#endif
	
	// obtencion de los resultados
	if (parametros.op == 2) {
		for (int i = 0; i < parametros.numero_threads; i++) {
			resultado_total = (int) resultado_total ^ (int) array_resultados[i];
		}
	} else {
		for (int i = 0; i < parametros.numero_threads; i++) {
			resultado_total += array_resultados[i];
		}
	}
	
	printf("El resultado del main es %f\n", resultado_total);
	
	tiempo_total = (fin.tv_sec - comienzo.tv_sec) * 1000000 + (fin.tv_usec - comienzo.tv_usec);
	printf("\nEl tiempo es %f microsegundos\n", tiempo_total);
	return estado;
}

void funcion(opciones *parametros, datos *elementos, datos_logger *elementos_logger, double *array_resultados, int posicion_thread) {
	double aux; // variable auxiliar 
	div_t division = div(parametros->tamanho_array,parametros->numero_threads);
	int cociente = division.quot; // carga de trabajo por thread
	
	// gestion si el numero de hilos es mayor que el tamanho del array
	if (parametros->numero_threads > parametros->tamanho_array) {
		if (parametros->op == 0 || parametros->op == 2) {
			array_resultados[posicion_thread] = elementos->array[posicion_thread];
		} else {
			aux = elementos->array[posicion_thread] * -1;
			array_resultados[posicion_thread] = aux;
		} 
	} else {
		// gestion del trabajo del array
		if (parametros->op == 0) {
			for (int i = posicion_thread * cociente; i < (posicion_thread + 1) * cociente; i++) {
				array_resultados[posicion_thread] += elementos->array[i];
			}
			
		} else if (parametros->op == 1) {
			for (int i = posicion_thread * cociente; i < (posicion_thread + 1) * cociente; i++) {
				aux = elementos->array[i] * -1;
				array_resultados[posicion_thread] += aux;
			}
		} else {
			for (int i = posicion_thread * cociente; i < (posicion_thread + 1) * cociente; i++) {
				array_resultados[posicion_thread] = (int)array_resultados[posicion_thread] ^ (int)elementos->array[i];
			}
		}
	// gestion del trabajo del array restante
	#if defined(FEATURE_OPTIMIZE)
		if (parametros->tamanho_array_restante > 0 && !g_atomic_restante.load()) {
			g_atomic_restante = true;
			if (parametros->op == 0) {
				for (int i = 0; i < parametros->tamanho_array_restante; i++) {
					array_resultados[posicion_thread] += elementos->array_restante[i];
				}
			} else if (parametros->op == 1) {
				for (int i = 0; i < parametros->tamanho_array_restante; i++) {
					aux = elementos->array_restante[i] * -1;
					array_resultados[posicion_thread] += aux;
				}
			} else {
				for (int i = 0; i < parametros->tamanho_array_restante; i++) {
					array_resultados[posicion_thread] = (int)array_resultados[posicion_thread] ^ (int)elementos->array_restante[i];
				}
			}
		}
	#else 
	{
		std::lock_guard<std::mutex> guard(g_m); // toma el mutex
		if (parametros->tamanho_array_restante > 0 && !g_restante_operado) {
			g_restante_operado = true;
			if (parametros->op == 0) {
				for (int i = 0; i < parametros->tamanho_array_restante; i++) {
					array_resultados[posicion_thread] += elementos->array_restante[i];
				}
			} else if (parametros->op == 1) {
				for (int i = 0; i < parametros->tamanho_array_restante; i++) {
					aux = elementos->array_restante[i] * -1;
					array_resultados[posicion_thread] += aux;
				}
			} else {
				for (int i = 0; i < parametros->tamanho_array_restante; i++) {
					array_resultados[posicion_thread] = (int)array_resultados[posicion_thread] ^ (int)elementos->array_restante[i];
				}
			}
		}
	}
	#endif
	}
	#if defined(FEATURE_LOGGER)
		{
			std::unique_lock<std::mutex> ulk(g_m2);
			g_c2.wait(ulk, []{return g_ready2;});
			// nuevos datos para el logger
			elementos_logger->posicion_thread = posicion_thread;
			elementos_logger->resultado_parcial = array_resultados[posicion_thread];
			g_ready = true; // permite que se despierte el logger
			g_ready2 = false; // hace esperar al siguiente hilo
			ulk.unlock();
		}
		g_c.notify_one(); // notifica al logger
	#endif
}

void funcion_logger(datos_logger *elementos_logger, opciones *parametros) {
	// bucle hasta trabajar todos los hilos
	while (elementos_logger->hilos_trabajados < parametros->numero_threads) {
		std::unique_lock<std::mutex> ulk(g_m2);
		g_c.wait(ulk, []{return g_ready;});
		
		g_ready = false; // hace esperar al logger
		g_ready2 = true; // permite que se despierte el siguiente hilo
		
		int posicion_thread = elementos_logger->posicion_thread; // numero de creacion del hilo 
		double resultado_parcial = elementos_logger->resultado_parcial; // resultado parcial del hilo
		elementos_logger->hilos_trabajados++;
		
		// obtencion de resultados parciales
		if (parametros->op == 0 || parametros->op == 1) {
			elementos_logger->resultados_logger[posicion_thread] += resultado_parcial;
		} else {
			elementos_logger->resultados_logger[posicion_thread] = (int)elementos_logger->resultados_logger[posicion_thread] ^ (int)resultado_parcial;
		}
		ulk.unlock();
		g_c2.notify_one();
	}
	
	// obtencion de los resultados totales
	for (int i = 0; i < parametros->numero_threads; i++) {
		#if defined(DEBUG)
			printf("Resultado parcial del hilo %d: %f\n", i, elementos_logger->resultados_logger[i]);
		#endif
		if (parametros->op == 0 || parametros->op == 1) {
			elementos_logger->resultado_total += elementos_logger->resultados_logger[i];
		} else {
			elementos_logger->resultado_total = (int)elementos_logger->resultado_total ^ (int)elementos_logger->resultados_logger[i];
		}	
	}
	// activacion del main
	{
		std::unique_lock<std::mutex> ulk(g_m3);
		elementos_logger->ready = true;
	}
	elementos_logger->c.notify_one();
}




