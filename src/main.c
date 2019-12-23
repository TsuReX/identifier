/**
 * @file	src/main.c
 *
 * @brief	Точка входа.
 *
 * @author	Vasily Yurchenko <vasily.v.yurchenko@yandex.ru>
 */

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <arpa/inet.h>

/** Неверное количество аргументов. */
#define	ERR_OPT						1
/** Ошибка установки обработчика сигналов. */
#define ERR_SIG_SETUP				2
/** Неверное состояние. */
#define ERR_INV_MODE				3
/** pid-файл существует, но не может быть открыт. */
#define ERR_PID_EXIST_CNTBEOPND		4
/** pid-файл существует, но не может быть прочитан. */
#define ERR_PID_EXIST_CNTBERD		5
/** Процесс существует в системе. */
#define ERR_PROC_EXIST				6
/** Запускаемый процесс уже существует в системе и имеет более высокие привелегии. */
#define ERR_SUPER_PROC_EXIST		7
/** Процесс был ранее некорректно завершен. */
#define ERR_INCORRECT_EXIT			8
/** Ошибка при отправке сигнала процессе с PID, прочитанным из pid-файла. */
#define ERR_SND_SIG					9
/** Ошибка создания pid-файла. */
#define ERR_CRT_PID					10
/** Ошибка создания процесса. */
#define ERR_CRT_PROC				11
/** Ошибка создания новой сессии. */
#define ERR_CRT_SES					12
/** Ошибка при записи pid-файла. */
#define ERR_CRT_PIDFILE				13
/** Произошла ошибка регистрации обработчика сигналов. */
#define ERR_REG_SIG					14
/** Произошла ошибка определения открытых дескрипторов. */
#define ERR_CHK_FD					15
/** Ошибка открытия pid-файла. */
#define ERR_PIDFILE					16
/** Произошла ошибка привязки порта к локальному адресу. */
#define ERR_BIND					17

/** Допустимое количество символов пллюс нулевой символ строкового представления PID. */
#define STR_PID_LEN		33
/** UDP порт на который осуществляется щироковещательная рассылка. */
#define UDP_PORT		12345

/** Флаг необходимости завершить работу программы. */
uint32_t exit_flag = 0;

/** TODO: Сделать описание. */
struct identifier {
	uint32_t value;
} __attribute__((packed));

/**
 * @brief	Выводит информацию о запуске и использовании программы.
 */
static void display_usage(void)
{
	printf("Вывод информации по использонию программы.\n");
	/* TODO: Реализовать. */
}

/**
 * @brief	Обрабатывает полученные сигналы.
 * 			При поступлении сигнала SIGINT происходит корректное завершение работы.
 * 			При поступлении иных сигналов просиходит принудительное завершение.
 */
static void daemon_signal_handle(int32_t sig_num) {

	switch (sig_num) {
		case SIGINT:
			exit_flag = 1;
			break;
		default:
			exit(-1);
	}
}

/**
 * @brief	Переводит процесс в состояние демона,
 * 			готовит UDP для передачи идентификационной информации.
 *
 * @return	0	успешно выполнение функции;
 * 			< 0	во время выполнения функции произошла ошибка (номер ошибки).
 */
static int daemon_start(void)
{
	/** Установить рабочую директорию "/" чтобы не зависеть от других директорий,
	 * которые могут удаляться/отмонтироваться во время работы. */
	chdir("/");

	/** Открыть pid-файл для сохранения идентификатора демона.
	 * Если файл существует, то прочитать содержащийся в нем PID,
	 * если процесс с таким PID есть в системе,
	 * то завершаем программу - в системе уже есть одна ее копия. */
	int pid_fd = open(PID_FILE, O_RDWR | O_CREAT | O_EXCL, 664);

	char str_pid[STR_PID_LEN] = "";

	if (pid_fd == -1) {
		/** pid-файл уже существует. */
		if (errno == EEXIST) {

			pid_fd = open(PID_FILE, O_RDONLY);

			if (pid_fd == -1) {
				printf("pid-файл существует, но не может быть открыт.\nУдалите pid-файл %s вручную.\n", PID_FILE);
				return -ERR_PID_EXIST_CNTBEOPND;
			}


			ssize_t read_cnt = read(pid_fd, str_pid, sizeof(str_pid) - 1);
			if (read_cnt == -1) {
				printf("pid-файл существует, но не может быть прочитан.\nУдалите pid-файл %s вручную.\n", PID_FILE);
				return -ERR_PID_EXIST_CNTBERD;
			}

			/* Функция не сигнализирует об ошибке в отличие от strtol */
			pid_t pid = atoi(str_pid);

			/** Проверить наличие процесса в системе отправкой нулевого сигнала. */
			if (kill(pid, 0) == 0) {

				/** Процесс существует в системе. */
				printf("Запускаемый процесс уже существует в системе.\n");
				return -ERR_PROC_EXIST;

			} else {

				if (errno == EPERM) {
					/** Процесс более приоритетный чтобы ему данный процесс отправил сигнал. */
					printf("Запускаемый процесс уже существует в системе и имеет более высокие привелегии.\n");
					return -ERR_SUPER_PROC_EXIST;

				} else if (errno == ESRCH) {
					/** Процесс не существует в системе. */
					printf("Процесс был ранее некорректно завершен.\nУдалите pid-файл %s вручную и повтоно запустите программу.\n", PID_FILE);
					return -ERR_INCORRECT_EXIT;

				} else {
					/** Ошибка отправки сигнала. */
					printf("Программа не может удостовериться, что в системе не запущена ее копия с PID %d.\n"
							"Убедитесь, что процесс не запущен, удалите pid-файл %s и повторите запуск приложения.\n", pid, PID_FILE);
					return -ERR_SND_SIG;

				}
			}

		/*if (errno == EEXIST)*/
		} else {
			/** Ошибка создания pid-файла. */
			printf("pid-файл %s не может быть создан, демон не будет запущен.\n", PID_FILE);
			/* А что если файл открыт и заблокирован на чтение? Такое возможно? */
			return -ERR_CRT_PID;
		}

	} /* open(PID_FILE, O_RDONLY) == -1 */

	/** Создать дочерний процесс и оставить только его.*/
	pid_t current_pid = fork();

	/** Дочерний процесс. */
	if (current_pid == 0) {

	/** Родительский процесс. */
	} else if (current_pid > 0) {
		exit(0);

	/** Ошибка создания процесса. */
	} else {
		printf("Произошла ошибка создания процесса.\n");
		close(pid_fd);
		remove(PID_FILE);
		return -ERR_CRT_PROC;
	}

	/** Файл создан, сохранить в него PID. Файл НЕ закрывать. */
	current_pid = getpid();
	snprintf(str_pid, sizeof(str_pid) - 1, "%d\n", current_pid);
	int wr_cnt = write(pid_fd, str_pid, strlen(str_pid));
	if (wr_cnt ==-1 || wr_cnt < strlen(str_pid)) {
		printf("Произошла ошибка при записи pid-файла.\n");
		close(pid_fd);
		remove(PID_FILE);
		return -ERR_CRT_PIDFILE;
	}

	/** Cоздать новый сеанс, чтобы не зависеть от родительского процесса. */
	if (setsid() < 0 ) {
		printf("Произошла ошибка создания нового сеанса.\n");
		close(pid_fd);
		remove(PID_FILE);
		return -ERR_CRT_SES;
	}

	/** Закрыть все дескрипторы кроме pid-файла. */
	int max_fd = sysconf(_SC_OPEN_MAX);
	if (max_fd == -1) {
		printf("Произошла ошибка определения открытых дескрипторов.\n");
		close(pid_fd);
		remove(PID_FILE);
		return -ERR_CHK_FD;
	}

	int fd;
	for (fd = 0; fd <= max_fd; ++fd) {
		if (fd != pid_fd)
			close(fd);
	}

	/** Зарегистрировать обработчики сигналов. */
	struct sigaction act;
	act.sa_handler = daemon_signal_handle;
	if (sigaction(SIGINT, &act, NULL) == -1) {
		printf("Произошла ошибка регистрации обработчика сигналов.\n");
		close(pid_fd);
		remove(PID_FILE);
		return -ERR_REG_SIG;
	}

	/** Получить идентификационную информацию. */
	/* TODO: Продумать, откуда будет процесс получать информаци. Реализовать. */

	/** Подготовить UDP, начать циклическую передачу. */
	/* TODO: Реализовать. */

	int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	/* TODO: Обработать возвращаемые значения. */

	int enable = 1;
	setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable));
	/* TODO: Обработать возвращаемые значения. */

	struct sockaddr_in broadcast_addr;

	size_t sockaddr_len = sizeof(struct sockaddr_in);

	struct identifier ident = {0xDEADBEEF};

	broadcast_addr.sin_family		= AF_INET;
	broadcast_addr.sin_port			= htons(UDP_PORT);
	broadcast_addr.sin_addr.s_addr	= INADDR_BROADCAST;

	while (exit_flag == 0) {
		sendto(sock_fd, &ident, sizeof(ident), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
		/* TODO: Обработать возвращаемые значения. */
		sleep(1);
	}
	close(sock_fd);

	/* TODO: После испытания UDP передачи убрать тестовый цикл. */
/*	remove("/tmp/test.txt");
	int test_fd = open("/tmp/test.txt", O_CREAT | O_RDWR);
	struct timespec time_val;
	char str_time_val[33];
	while(exit_flag == 0) {
		clock_gettime(CLOCK_REALTIME, &time_val);
		snprintf(str_time_val, sizeof(str_time_val) - 1, "%ld\n", time_val.tv_sec);
		write(test_fd, str_time_val, strlen(str_time_val));
		sleep(1);
	}
	close(test_fd);
	remove("/tmp/test.txt");
*/
	/** Закрыть и удалить pid-файл. */
	close(pid_fd);
	remove(PID_FILE);
}

/**
 *
 */
static int client_start(void)
{
	int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	/* TODO: Обработать возвращаемые значения. */

	int enable = 1;
	setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable));
	/* TODO: Обработать возвращаемые значения. */

	struct sockaddr_in remote_addr;
	struct sockaddr_in client_addr;
	struct identifier ident;

	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(UDP_PORT);
	client_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock_fd, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
		printf("Произошла ошибка привязки порта к локальному адресу.");
		close(sock_fd);
		return -ERR_BIND;
	}
	printf("Порт привязан к адресу.\n");
	int len = sizeof(struct sockaddr_in);

	struct pollfd fds[1];

	fds[0].fd = sock_fd;
	fds[0].events = POLLIN;

	int ret_val;

	while (exit_flag == 0) {

		ret_val = poll(fds, 1, 1000);

		if (ret_val == -1) {
			/* TODO: Обработать ошибки. */
			perror("Ошибка опроса сетевого порта");
		} else if (ret_val == 0) {
			/* TODO: Обработать таймаут. */
			printf("Ничего не было принято.\n");
		} else {
			if (fds[0].revents & POLLIN) {
				fds[0].revents = 0;
				/* TODO: Принять поступившие данные - recvfrom(). */
				recvfrom(sock_fd, &ident, sizeof(struct identifier), 0, (struct sockaddr*) &remote_addr, &len);

				char client_str_ip[32] = "client_ip";
				int client_ip = ntohl(client_addr.sin_addr.s_addr);

				char remote_str_ip[32] = "remote_ip";
				int remote_ip = ntohl(remote_addr.sin_addr.s_addr);

				inet_ntop(AF_INET, &client_ip, client_str_ip, sizeof(struct sockaddr*));
				inet_ntop(AF_INET, &remote_ip, remote_str_ip, sizeof(struct sockaddr*));

				printf("Локальный адрес: %s, удаленный адрес: %s, идентификатор: 0x%08X\n", client_str_ip, remote_str_ip, ident.value);
			}
		}
	}
	close(sock_fd);

	return 0;
}

/**
 * @brief	Определяет запущен ли в системе демон и отправляет ему сигнал завершения работы.
 *
 * @return	0	успешно выполнение функции;
 * 			< 0	во время выполнения функции произошла ошибка (номер ошибки).
 */
static int service_start(void)
{

	/** Открыть pid-файл. */
	int pid_fd = open(PID_FILE, O_RDONLY);
	if (pid_fd == -1) {
		/* TODO: Обработать различные возвращаемые значения чтобы различать отсутствие файла, нехватку прав на доступ и т.п.. */
		printf("Невозможно открыть файл %s.\n", PID_FILE);
		return -ERR_PIDFILE;
	}

	char str_pid[STR_PID_LEN] = "";

	/** Прочитать значение PID. */
	int rd_cnt = read(pid_fd, str_pid, STR_PID_LEN);
	/* TODO: Обработать возвращаемые значения. */

	int daemon_pid = atoi(str_pid);
	printf("PID: %d\n", daemon_pid);

	/** Отправить сигнал завершения. */
	if (kill(daemon_pid, SIGINT) == -1) {

		if (errno == EPERM) {
			/** Процесс более приоритетный чтобы ему данный процесс отправил сигнал. */
			printf("Демон имеет более высокие привелегии.\n");
			return -ERR_SUPER_PROC_EXIST;

		} else if (errno == ESRCH) {
			/** Процесс не существует в системе. */
			printf("Процесс был ранее некорректно завершен.\nУдалите pid-файл %s вручную и повтоно запустите программу.\n", PID_FILE);
			return -ERR_INCORRECT_EXIT;

		} else {
			/** Ошибка отправки сигнала. */
			printf("Ошибка отправки сигнала демону %d.\n", daemon_pid);
			return -ERR_SND_SIG;
		}
	}
	/** Подождать некоторое время. */
	sleep(5);

	/** Проверить отсутствие процесса в системе и выдать соответствующее сообщение. */
	if (kill(daemon_pid, 0) == 0) {

		/** Процесс существует в системе. */
		printf("Не удалось завершить демон.\n");
		return -ERR_PROC_EXIST;

	} else {

		if (errno == EPERM) {
			/** Процесс более приоритетный чтобы ему данный процесс отправил сигнал. */
			printf("Демон имеет более высокие привелегии.\n");
			return -ERR_SUPER_PROC_EXIST;

		} else if (errno == ESRCH) {
			/** Процесс не существует в системе. */
			printf("Демон завершен.\n");

		} else {
			/** Ошибка отправки сигнала. */
			printf("Ошибка отправки сигнала демону %d.\n", daemon_pid);
			return -ERR_SND_SIG;

		}
	}

	/** Закрыть pid-файл. */
	close(pid_fd);

	return 0;
}

/**
 * @brief	Точка входа в программу.
 */
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

	if (argc == 1 || argc > 2) {
		printf("Программа требует обязятельного указания одного аргумента.\n");
		display_usage();
		return -ERR_OPT;
	}

	/** Перечисление режимов работы программы. */
	enum mode {
		unknown = 0,	/**< Начальный неопределенный режим работы программы. */
		daemon,			/**< Режим демона, в котором происходит циклическая передача. */
		client,			/**< Режим клиента, в котором происходит прием.*/
		service			/**< Сервисный режим, в котором происходит остановка демона.*/
	};

	enum mode cur_mode = unknown;

	const char options[] = "dcs";
	int opt = getopt(argc, argv, options);
	while (opt != -1) {
		switch (opt) {
		case 'd':
			cur_mode = daemon;
			break;

		case 'c':
			cur_mode = client;
			break;

		case 's':
			cur_mode = service;
			break;

		case 'h':
		case '?':
		default:

			display_usage();
			break;
		}

		/** Выход из цикла while (opt != -1),так как программа
		 * в настоящее время может обработать только один параметр. */
		break;
	}

	switch (cur_mode) {
		case daemon:
			printf("Активирован режим демона.\n");
			daemon_start();
			break;

		case client:
			printf("Активирован режим клиента.\n");
			client_start();
			break;

		case service:
			printf("Активирован сервисный режим.\n");
			service_start();
			break;

		default:

			return -ERR_INV_MODE;
	}

	return 0;
}
