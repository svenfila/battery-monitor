#include <ncurses.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>

static unsigned int SCREEN_HEIGHT = 24;

static const unsigned int OFFSET_LEFT = 10;
static const unsigned int OFFSET_BOTTOM = 3;
static unsigned int OFFSET_TOP;

static unsigned int BAR_WIDTH = 3;
static int SPACE_BETWEEN_BARS = 3;

static unsigned int VOLTS_MIN = 80;
static unsigned int VOLTS_MAX = 150;
static double VOLTS_STEP;

static unsigned int MAX_LINE_LENGTH = 512;

static const char* ALLOWED_CHARS = { "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ," };

static unsigned int FRAME_INTERVAL = 0;

static char* OUTPUT_FILE = NULL;

// Functions
void init_screen();
void finish_screen(int sig);
void finish_screen_and_exit(int sig);
int bar_y(unsigned int positions_up_from_bottom);
int bar_x(unsigned int bar_position, unsigned int bar_width);
void move_cursor_to_bottom_line();
void print_left_panel();
void print_bottom_panel(int battery_count);
void print_battery_bars(unsigned int battery_size, int batteries[]);
//void print_status_message(char * message);
int round_to_int(double x);
void set_options(int argc, char** argv);
FILE* open_file(char* fileName, char* mode);
void close_file(FILE* file);
void read_voltages(char line[], int* voltages);
bool is_valid_line(char line[]);
// End of functions

void init_screen() {
  initscr();
  signal(SIGINT, finish_screen_and_exit);
  keypad(stdscr, true);
  nonl();
  cbreak();

  if (has_colors()) {
    start_color();

    init_pair(COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
    init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
    init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  }
}

void finish_screen(int sig) {
  endwin();

  if (sig != 0) {
    printf("Got signal %d\n", sig);
  }
}

void finish_screen_and_exit(int sig) {
  finish_screen(sig);
  exit(EXIT_SUCCESS);
}

int bar_y(unsigned int positions_up_from_bottom) {
  return SCREEN_HEIGHT - OFFSET_BOTTOM - positions_up_from_bottom;
}

int bar_x(unsigned int bar_position, unsigned int bar_width) {
  return 1 + OFFSET_LEFT + (SPACE_BETWEEN_BARS + BAR_WIDTH) * (bar_position) + bar_width;
}

void move_cursor_to_bottom_line() {
  move(bar_y(-2), 0);
}

void print_left_panel() {
  attron(COLOR_PAIR(COLOR_CYAN));
  attron(A_BOLD);
  mvprintw(0, OFFSET_LEFT - 6, "Volts:");
  attroff(A_BOLD);

  int i;
  for (i = 0; i <= OFFSET_TOP - OFFSET_BOTTOM; i += 2) {
    mvprintw(bar_y(i), OFFSET_LEFT - 6, "%5.2f", (VOLTS_MIN + i * VOLTS_STEP)
        / 10.0);
  }

  attron(A_BOLD);
  mvprintw(bar_y(-1), 1, "Battery:");
  attroff(A_BOLD);
  attroff(COLOR_PAIR(COLOR_CYAN));

  move_cursor_to_bottom_line();
}

void print_bottom_panel(int battery_count) {
  int i;

  attron(COLOR_PAIR(COLOR_CYAN));
  for (i = 0; i < battery_count; i++) {
    mvprintw(bar_y(-1), bar_x(i, 0), "%2d", i + 1);
  }
  attroff(COLOR_PAIR(COLOR_CYAN));
}

void print_battery_bars(unsigned int battery_size, int batteries[]) {
  int i, j, k;

  for (i = 0; i < battery_size; i++) {
    if (batteries[i] < VOLTS_MIN) {
      batteries[i] = VOLTS_MIN;
    }
    if (batteries[i] > VOLTS_MAX) {
      batteries[i] = VOLTS_MAX;
    }

    int current = 1 + round_to_int((batteries[i] - VOLTS_MIN) / VOLTS_STEP);

    attron(A_REVERSE);
    attron(COLOR_PAIR(COLOR_WHITE));
    for (j = 0; j < current; j++) {
      for (k = 0; k < BAR_WIDTH; k++) {
        mvaddch(bar_y(j), bar_x(i, k), ' ');
      }
    }
    attroff(COLOR_PAIR(COLOR_WHITE));
    attroff(A_REVERSE);

    for (j = current; j <= OFFSET_TOP - OFFSET_BOTTOM; j++) {
      for (k = 0; k < BAR_WIDTH; k++) {
        mvaddch(bar_y(j), bar_x(i, k), ' ');
      }
    }
  }

  move_cursor_to_bottom_line();
}

//void print_status_message(char * message) {
//  mvprintw(bar_y(-2), 1, message);
//}

int round_to_int(double x) {
  double half;
  if (x > 0) {
    half = 0.5;
  } else {
    half = -0.5;
  }

  return (int) (x + half);
}

void print_help(char* program_name) {
  printf("Usage: %s SOURCE [options]...\n\n", program_name);
  printf("  --output-file=FILE           append input file lines to this file\n");
  printf("  --screen-height=NUMBER       screen height, in lines\n");
  printf("  --bar-width=NUMBER           voltage value bar width, in columns\n");
  printf("  --space-between-bars=NUMBER  space between voltage value bars,\n");
  printf("                               in columns\n");
  printf("  --volts-min=NUMBER           min voltage value used on the screen,\n");
  printf("                               in volts\n");
  printf("  --volts-max=NUMBER           max voltage value used on the screen,\n");
  printf("                               in volts\n");
  printf("  --max-line-length=NUMBER     max length of line that is read from\n");
  printf("                               the data file, in bytes\n");
  printf("  --frame-interval=NUMBER      time interval between displaying next\n");
  printf("                               frame, in milliseconds\n");
}

void set_options(int argc, char** argv) {
  static struct option long_options[] = {
      { "screen-height", 1, 0, 1 },
      { "bar-width", 1, 0, 2 },
      { "space-between-bars", 1, 0, 3 },
      { "volts-min", 1, 0, 4 },
      { "volts-max", 1, 0, 5 },
      { "max-line-length", 1, 0, 6 },
      { "frame-interval", 1, 0, 7 },
      { "output-file", 1, 0, 8 },
      { "help", 0, 0, 9 },
      { 0, 0, 0, 0 }
  };

  bool failure = false;

  for (;;) {
    int option_index = 0;

    int c = getopt_long_only(argc, argv, "", long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
    case 1:
      SCREEN_HEIGHT = atoi(optarg);
      break;

    case 2:
      BAR_WIDTH = atoi(optarg);
      break;

    case 3:
      SPACE_BETWEEN_BARS = atoi(optarg);
      break;

    case 4:
      VOLTS_MIN = 10 * atoi(optarg);
      break;

    case 5:
      VOLTS_MAX = 10 * atoi(optarg);
      break;

    case 6:
      MAX_LINE_LENGTH = atoi(optarg);
      break;

    case 7:
      FRAME_INTERVAL = 1000 * atoi(optarg);
      break;

    case 8:
      OUTPUT_FILE = optarg;
      break;

    case 9:
      failure = true;
      break;

    case '?':
      failure = true;
      break;

    default:
      failure = true;
    }
  }

  if (failure) {
    print_help(*argv);
    exit(EXIT_FAILURE);
  }

  OFFSET_TOP = SCREEN_HEIGHT - 1;
  VOLTS_STEP = (double) (VOLTS_MAX - VOLTS_MIN) / (OFFSET_TOP - OFFSET_BOTTOM);
}

FILE* open_file(char* fileName, char* mode) {
  FILE* file = fopen(fileName, mode);
  if (file == NULL) {
    finish_screen(0);
    perror("Failed to open file");
    exit(EXIT_FAILURE);
  }

  return file;
}

void close_file(FILE* file) {
  fclose(file);
}

void read_voltages(char* line, int* voltages) {
  int* voltages_offset = voltages;

  bool battery_data_zone = false;
  char* token = strtok(line, ",");

  do {
    if (*token == 'H') {
      battery_data_zone = false;
    }

    if (battery_data_zone) {
      int x = atoi(token);
      *voltages_offset++ = x;
    }

    if (*token == 'B') {
      battery_data_zone = true;
    }
  } while (NULL != (token = strtok(NULL, ",")));

  *voltages_offset++ = -1;
}

void append_data_line(FILE* file, char* line) {
  if (EOF == fputs(line, file) || EOF == fputc('\n', file)) {
    finish_screen(0);
    perror("Failed to write to file");
    exit(EXIT_FAILURE);
  }
}

bool is_valid_line(char* line) {
  bool valid_line = true;

  char* tmp = malloc(strlen(line) + 1);

  int i, j;
  // strip white space
  for (i = 0, j = 0; line[i] != '\0'; i++) {
    if (NULL == strchr(" \t\r\n", line[i])) {
      tmp[j++] = line[i];
    }
  }
  tmp[j] = '\0';

  if (j == 0) {
    valid_line = false;
  }

  // check that all characters are among ALLOWED_CHARS
  for (i = 0; tmp[i] != '\0'; i++) {
    if (NULL == strchr(ALLOWED_CHARS, tmp[i])) {
      valid_line = false;
      break;
    }
  }

  if (valid_line) {
    strcpy(line, tmp);
  }

  free(tmp);

  return valid_line;
}

int main(int argc, char** argv) {
  if (argc == 1) {
    printf("Supply a file name\n");
    exit(EXIT_FAILURE);
  }
  char* fileName = argv[1];

  set_options(argc, argv);

  init_screen();

  print_left_panel();
  refresh();

  FILE* outFile;
  if (OUTPUT_FILE != NULL) {
    outFile = open_file(OUTPUT_FILE, "a");
  }

  int voltages[25];
  char line[MAX_LINE_LENGTH];

  for (;;) {
    FILE* dataFile = open_file(fileName, "r");

    while (fgets(line, sizeof(line), dataFile)) {
      if (is_valid_line(line)) {
        if (OUTPUT_FILE != NULL) {
          append_data_line(outFile, line);
        }
        read_voltages(line, voltages);

        int i;
        int batteries_size;
        for (i = 0; i < sizeof(voltages) / sizeof(int); i++) {
          if (voltages[i] == -1) {
            batteries_size = i;
            break;
          }
        }

        int batteries[batteries_size];
        memcpy(batteries, voltages, batteries_size * sizeof(int));

        print_bottom_panel(batteries_size);
        print_battery_bars(batteries_size, batteries);
        refresh();

        if (FRAME_INTERVAL > 0) {
          usleep(FRAME_INTERVAL);
        }
      }
    }

    int isEof = feof(dataFile);

    close_file(dataFile);

    if (isEof) {
      break;
    }
  }

  getch();

  if (OUTPUT_FILE != NULL) {
    close_file(outFile);
  }

  finish_screen(0);
  return EXIT_SUCCESS;
}
