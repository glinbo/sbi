#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* CELL OPTIONS */
#define CELL_WRAP   0
#define CELL_CLAMP  1
#define CELL_QUIT   2
int cell_behaviour = CELL_WRAP;
int cell_min = 0;
int cell_max = 255;

/* BUFFER SETTINGS */
#define BUF_WRAP    0
#define BUF_CLAMP   1
#define BUF_QUIT    2 
int buffer_behaviour = BUF_WRAP;
int b_length = 30000;
int *buffer = NULL;

/* PROGRAM INSTRUCTIONS */
int c_length;
char *code = NULL;

/* EOF OPTIONS */
#define EOF_VALUE   0
#define EOF_NOVALUE 1
#define EOF_QUIT    2
int eof_behaviour = EOF_VALUE;
int eof_value = -1;

/* I/O OPTIONS */
int file = 0;
char *filename = NULL;
int flush_stdout = 0;
int separator = '!';
/* NO MORE OPTIONS */



/* modify value so that:
   i)  newvalue is in [min, max] interval
   ii) newvalue == oldvalue modulo interval length
 */
int normalize(int value, int min, int max)
{
	if (value < min)
		return max - (min - value - 1) % (max - min + 1);
	if (value > max)
		return min + (value - max - 1) % (max - min + 1);
	return value;
}

int setup_code_file()
{
	int fd, pos, tmp;

	if ((fd = open(filename, O_RDONLY)) == -1) {
		perror("ERROR - open");
		return -1;
	}
	if ((c_length = lseek(fd, 0, SEEK_END)) == (off_t)-1) {
		perror("ERROR - lseek");
		close(fd);
		return -1;
	}
	if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
		perror("ERROR - lseek");
		close(fd);
		return -1;
	}
	if (!(code = malloc(c_length))) {
		perror("ERROR - malloc");
		close(fd);
		return -1;
	}
	for (pos = 0; pos < c_length; pos += tmp) {
		tmp = read(fd, &code[pos], c_length - pos);
		switch (tmp) {
		case 0:
			fprintf(stderr, "ERROR - could not read file entirely");
			free(code);
			close(fd);
			return -1;
		case -1:
			perror("ERROR - read");
			free(code);
			close(fd);
			return -1;
		}
	}
	if (close(fd)) {
		/* wat */
		perror("ERROR - close");
		free(code);
		return -1;
	}
	return 0;
}

int setup_code_stdin()
{
	int c;
	int size = 30000;

	if (!(code = malloc(size))) {
		perror("ERROR - malloc");
		return -1;
	}
	for (c_length = 0; (c = getchar()) != EOF; ++c_length) {
		if (c == separator)
			break;
		if (c_length >= size) {
			char *tmp;
			size *= 2;
			if (!(tmp = realloc(code, size))) {
				perror("ERROR - realloc");
				free(code);
				return -1;
			}
			code = tmp;
		}
		code[c_length] = c;
	}

	return 0;
}


int setup_buffer()
{
	if (!(buffer = malloc(b_length * sizeof(*buffer)))) {
		perror("ERROR - malloc");
		return -1;
	}
	memset(buffer, b_length * sizeof(*buffer), 0);
	return 0;
}

int bf_exec()
{
	int ip, bp, tmp;

	for (ip = 0, bp = 0; ip < c_length; ++ip) {
		switch (code[ip]) {
		case '>':
			++bp;
			if (bp >= b_length) {
				switch (buffer_behaviour) {
				case BUF_WRAP:
					bp = 0;
					break;
				case BUF_CLAMP:
					bp = b_length - 1;
					break;
				case BUF_QUIT:
					fprintf(stderr, "ERROR: (bp) overflow\n");
					return -1;
				}
			}
			break;
		case '<':
			--bp;
			if (bp < 0) {
				switch (buffer_behaviour) {
				case BUF_WRAP:
					bp = b_length - 1;
					break;
				case BUF_CLAMP:
					bp = 0;
					break;
				case BUF_QUIT:
					fprintf(stderr, "ERROR: (bp) underflow\n");
					return -1;
				}
			}
			break;
		case '+':
			++buffer[bp];
			if (buffer[bp] > cell_max) {
				switch (cell_behaviour) {
				case CELL_WRAP:
					buffer[bp] = cell_min;
					break;
				case CELL_CLAMP:
					buffer[bp] = cell_max;
					break;
				case CELL_QUIT:
					fprintf(stderr, "ERROR: #%d cell overflow\n", bp);
					return -1;
				}
			}
			break;
		case '-':
			--buffer[bp];
			if (buffer[bp] < cell_min) {
				switch (cell_behaviour) {
				case CELL_WRAP:
					buffer[bp] = cell_max;
					break;
				case CELL_CLAMP:
					buffer[bp] = cell_min;
					break;
				case CELL_QUIT:
					fprintf(stderr, "ERROR: #%d cell underflow\n", bp);
					return -1;
				}
			}
			break;
		case '.':
			putchar(buffer[bp]);
			if (flush_stdout)
				fflush(stdout);
			break;
		case ',':
			if ((tmp = getchar()) == EOF) {
				switch(eof_behaviour) {
				case EOF_VALUE:
					buffer[bp] = normalize(eof_value, cell_min, cell_max);
					break;
				case EOF_NOVALUE:
					break;
				case EOF_QUIT:
				default:
					fprintf(stderr, "<EOF>\n");
					return 0;
				}
			}
			else
				buffer[bp] = normalize(tmp, cell_min, cell_max);
			break;
		case '[':
			if (!buffer[bp]) {
				for (tmp = 1; ip < c_length - 1 && tmp > 0; ) {
					switch (code[++ip]) {
					case '[':
						++tmp;
						break;
					case ']':
						--tmp;
						break;
					}
				}
				if (tmp) {
					fprintf(stderr, "ERROR: unmatched '['\n");
					return -1;
				}
			}
			break;
		case ']':
			if (buffer[bp]) {
				for (tmp = 1; ip > 0 && tmp > 0; ) {
					switch (code[--ip]) {
					case '[':
						--tmp;
						break;
					case ']':
						++tmp;
						break;
					}
				}
				if (tmp) {
					fprintf(stderr, "ERROR: unmatched ']'\n");
					exit(-1);
				}
			}
			break;
		}
	}
	return 0;
}


int parse_options(int argc, char** argv)
{
	int c;
	char* err;

	while ((c = getopt(argc, argv, ":hf:l:u:c:k:b:e:s:o")) != -1) {
		switch (c) {
		case 'h':
			printf(
			"Simple brainfuck interpreter\n\n"
			"I/O OPTIONS:\n"
			"  -f <file>: file to parse (default: <stdin>)\n"
			"  -s <char>: program/stdin separator (used when interpreting from stdin) (default: '!')\n"
			"  -o: flush stdout after every char outputed\n"
			"CELL OPTIONS:\n"
			"  -l <num>: lower cell bound (default: 0)\n"
			"  -u <num>: upper cell bound (default: 255)\n"
			"  -c wrap|clamp|quit: cell under/overflow behaviour (default: wrap)\n"
			"BUFFER OPTIONS:\n"
			"  -k <size>: buffer size (default: 30000)\n"
			"  -b wrap|clamp|quit: buffer under/overflow behaviour (default: wrap)\n"
			"EOF OPTIONS:\n"
			"  -e <num>|none|quit: eof behaviour (default: -1)\n"
			"MISC OPTIONS:\n"
			"  -h: help\n"
			);
			exit(0);
		case 'f':
			if (!strcmp(optarg, "-"))
				file = 0;
			else {
				filename = optarg;
				file = 1;
			}
			break;
		case 'l':
			cell_min = strtol(optarg, &err, 10);
			if (err == optarg) {
				fprintf(stderr, "ERROR: '-l' incorrect value (%s)\n", optarg);
				exit(-1);
			}
			break;
		case 'u':
			cell_max = strtol(optarg, &err, 10);
			if (err == optarg) {
				fprintf(stderr, "ERROR: '-u' incorrect value (%s)\n", optarg);
				exit(-1);
			}
			break;
		case 'c':
			switch (optarg[0]) {
			case 'w':
				cell_behaviour = CELL_WRAP;
				break;
			case 'c':
				cell_behaviour = CELL_CLAMP;
				break;
			case 'q':
				cell_behaviour = CELL_QUIT;
				break;
			default:
				fprintf(stderr, "ERROR: incorrect '-c' parameter (%s)\n", optarg);
				exit(-1);
			}
			break;
		case 'k':
			b_length = strtol(optarg, &err, 10);
			if (err == optarg || b_length <= 0) {
				fprintf(stderr, "ERROR: incorrect '-k' parameter (%s)\n", optarg);
				exit(-1);
			}
			break;
		case 'b':
			switch (optarg[0]) {
			case 'w':
				buffer_behaviour = BUF_WRAP;
				break;
			case 'c':
				buffer_behaviour = BUF_CLAMP;
				break;
			case 'q':
				buffer_behaviour = BUF_QUIT;
				break;
			default:
				fprintf(stderr, "ERROR: incorrect '-b' parameter (%s)\n", optarg);
				exit(-1);
			}
			break;
		case 'e':
			printf("D: \"%s\"\n", optarg);
			switch (optarg[0]) {
			case 'n':
				eof_behaviour = EOF_NOVALUE;
				break;
			case 'q':
				eof_behaviour = EOF_QUIT;
				break;
			default:
				eof_behaviour = EOF_VALUE;
				eof_value = strtol(optarg, &err, 10);
				if (err == optarg) {
					fprintf(stderr, "ERROR: incorrect '-e' parameter (%s)\n", optarg);
					exit(-1);
				}
			}
			break;
		case 's':
			if (strchr("><+-.,[]", optarg[0])) {
				fprintf(stderr, "ERROR: incorrect '-s' parameter: '%s' cannot be used as separator\n", optarg);
				exit(-1);
			}
			separator = optarg[0];
			break;
		case 'o':
			flush_stdout = 1;
			break;
		/* should I set `opterr' to 0? */
		case '?':
			fprintf(stderr, "ERROR: '-%c' option not recognized\n", optopt);
			exit(-1);
		case ':':
			fprintf(stderr, "ERROR: '-%c' requires parameter\n", optopt);
			exit(-1);
		}
	}
	if (cell_min > cell_max) {
		fprintf(stderr, "ERROR: cell_min > cell_max (%d > %d)\n", cell_min, cell_max);
		exit(-1);
	}
	return 0;
}

int main(int argc, char** argv)
{
	parse_options(argc, argv);
	if (file ? setup_code_file():setup_code_stdin())
		return -1;
	if (setup_buffer()) {
		free(code);
		return -1;
	}
	if (bf_exec()) {
		free(code);
		free(buffer);
		return -1;
	}
	free(code);
	free(buffer);
	return 0;
}

