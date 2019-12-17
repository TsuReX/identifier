/**
 * @file	src/main.c
 *
 * @brief	Точка входа.
 *
 * @author	Vasily Yurchenko <vasily.v.yurchenko@yandex.ru>
 *
 *
 *
 */

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>

/**
 *
 */
static void display_usage(void)
{
	printf("Вывод информации по использонию программы.\n");
	/* TODO: Реализовать. */
}

uint32_t exit_flag = 0;
static void handleSignal(int32_t sigNum) {

	switch (sigNum) {
		case SIGINT:
		printf("Someone wants to stop application\n");
		exit_flag = 1;
		break;
	default:
		printf("Unregistered signal received: %d\n", sigNum);
		exit(-1);
		break;
	}
}

/**
 *
 */
static int sig_handler_init(void)
{
	/* TODO: Реализовать. */
	struct sigaction act;
	act.sa_handler = handleSignal;
	return sigaction(SIGINT, &act, NULL);
}

/**
 *
 */
static int daemon_start(void)
{
	/* TODO: Реализовать. */
}

/**
 *
 */
static int client_start(void)
{
	/* TODO: Реализовать. */
}

/**
 *
 */
static int service_start(void)
{
	/* TODO: Реализовать. */
}

int main(uint32_t argc, char *argv[])
{
	/* Последовательность действий:
	 * 1. Обработать параметры командной строки:
	 *
	 * 		-d запускается процесс в режиме демона,
	 * 		начинает в цикле передавать идентификационную
	 * 		информацию по UDP, запустить несколько демонов нельзя;
	 *
	 * 		-с запускается процесс, который осуществляет
	 * 		прием идентификационных данных по UDP;
	 *
	 * 		-s завершить запущенный процесс-демон.
	 *
	 * 2. Если активирован режим демона, то:
	 *
	 *		процесс переходит в режим демона;
	 *
	 *		происходит запись своего PID;
	 *
	 *		устанавливается обработчик сигналов завершения;
	 *
	 *		настраивается UDP порт передачи идентификационной информации;
	 *
	 *		запускается циклическая передача с проверкой флага завершения.
	 *
	 *	в случае поступления сигнала завершения устанавливается флаг завершения
	 *	для корректного освобождения ресурсов и завершения работы.
	 *
	 * 3. Если запущен процесс-приемник идентификационной информации, то:
	 *
	 * 		устанавливается обработчик сигналов завершения;
	 *
	 * 		настраивается UDP порт для приема;
	 *
	 * 		запускается циклический прием идентификационных данных  с проверкой флага завершения;
	 *
	 * 			при поступлении идентификационного пакета он разбирается и
	 * 			данные добавляются в список, если их там еще нет;
	 *
	 * 			в случае непоступления идентификационных данных некоторое время и
	 * 			при их наличие в списке, они оттуда удаляются.
	 *
	 * 		происходит вывод списка в консоль.
	 *
	 * 	в случае поступления сигнала завершения устанавливается флаг завершения
	 *	для корректного освобождения ресурсов и завершения работы.
	 *
	 *	4. Если выбрано завершение демона, то:
	 *
	 *		открывается файл /var/run/identifier.pid, прочитать PID демона;
	 *
	 *		отправить сообщение о завершении работы процессу с идентификатором PID.
	 */

	/** Перечисление режимов работы программы. */
	enum mdoe {
		unknown = 0,	/**< Начальный неопределенный режим работы программы. */
		daemon,			/**< Режим демона, в котором происходит циклическая передача. */
		client,			/**< Режим клиента, в котором происходит прием.*/
		service			/**< Сервисный режим, в котором происходит остановка демона.*/
	};

	enum mode cur_mode = unknown;

	const char options[] = "dcs";
	int opt = getopt( argc, argv, options );
	while (opt != -1) {
		switch (opt) {
			case 'd':
				cur_mode = daemon;
				printf("Активирован режим демона.\n");
				break;

			case 'c':
				cur_mode = client;
				printf("Активирован режим клиента.\n");
				break;

			case 'k':
				cur_mode = service;
				printf("Активирован сервисный режим.\n");
				break;

			case 'h':
			case '?':
			default:

				display_usage();
				break;
		}
		if (cur_mode != unknown)
			break;
	}

	return 0;
}
