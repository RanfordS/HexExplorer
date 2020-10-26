#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <sys/ioctl.h>



typedef union
{
    float f;
    char b[sizeof(float)];
}
bytefloat_t;

typedef union
{
    double d;
    char b[sizeof(double)];
}
bytedouble_t;

typedef union
{
    lua_Integer i;
    char b[sizeof(lua_Integer)];
}
bytelint_t;



FILE* file;
long int file_length;
lua_State* luaState;

int term;
struct winsize term_size;

#define HISTORY_SIZE 128
#define HISTORY_LOG_SIZE 128
long int history_filepos[HISTORY_SIZE] = {};
char history_log[HISTORY_SIZE][HISTORY_LOG_SIZE] = {};
unsigned int history_start = 0;
unsigned int history_end = 0;
#define HISTORY (history_log[history_end] + bufferpos)

#define REPORT_SIZE 64
int report_error = 0;
char report[REPORT_SIZE] = {};

#define ERROR(s) {sprintf (report, "[!] %s", s); report_error = 1;}

#define MIN(a,b) ((a) < (b) ? (a) : (b))

int running = 1;
int l_quit (lua_State* L)
{
    running = 0;
    return 0;
}

int l_clear (lua_State* L)
{
    history_start = 0;
    history_end = 0;
    return 0;
}

int l_print (lua_State* L)
{
    const char* str = lua_tostring (L, 1);
    strncpy (report, str, REPORT_SIZE);
}

int l_seek (lua_State* L)
{
    if (!lua_isinteger (L, 1))
    {
        ERROR("arg 1 of seek was not an integer");
        return 0;
    }

    lua_Integer i = lua_tointeger (L, 1);
    if (i < 0 || file_length <= i)
    {
        ERROR("arg 1 of seek exceeds file bounds");
        return 0;
    }

    fseek (file, i, SEEK_SET);
    return 0;
}

#define STACK_SIZE 64
#define STACK_NOTE 32
long int stack[STACK_SIZE] = {};
char stack_notes[STACK_SIZE][STACK_NOTE] = {};
unsigned int stack_elements = 0;

int l_push (lua_State* L)
{
    if (stack_elements >= STACK_SIZE)
    {
        ERROR("no space on stack to push");
        return 0;
    }

    if (lua_isstring (L, 1))
    {
        const char* str = lua_tostring (L, 1);
        strncpy (stack_notes[stack_elements], str, STACK_NOTE-1);
    }
    else
    {
        stack_notes[stack_elements][0] = 0;
    }

    stack[stack_elements++] = ftell (file);

    return 0;
}

int l_back (lua_State* L)
{
    if (stack_elements > 0)
    {
        fseek (file, stack[stack_elements-1], SEEK_SET);
    }
    else
    {
        fseek (file, 0, SEEK_SET);
    }
    return 0;
}

int l_pop (lua_State* L)
{
    lua_Integer i = 1;
    if (lua_isinteger (L, 1))
    {
        i = lua_tointeger (L, 1);
    }

    if (i <= 0)
    {
        ERROR("arg 1 of pop must be strictly positive");
        return 0;
    }

    if (stack_elements < i)
    {
        ERROR("pop count exceeds the number of stack entries");
        return 0;
    }

    stack_elements -= i;
    fseek (file, stack[stack_elements], SEEK_SET);

    return 0;
}

int l_delete (lua_State* L)
{
    lua_Integer i = 1;
    if (lua_isinteger (L, 1))
    {
        i = lua_tointeger (L, 1);
    }

    if (i < 0)
    {
        ERROR("arg 1 of delete must be positive");
        return 0;
    }

    int diff = (int)history_end - history_start;
    if (diff < 0) diff += HISTORY_SIZE;

    if (i > diff)
    {
        ERROR("arg 1 of delete exceeds the number of history elements");
        return 0;
    }

    history_end = (history_end - i) % HISTORY_SIZE;
    return 0;
}

int l_note (lua_State* L)
{
    const char* str = lua_tostring (L, 1);

    int bufferpos = 0;
    strncpy (HISTORY, str, HISTORY_LOG_SIZE);
    history_filepos[history_end] = ftell (file);

    history_end = (history_end + 1) % HISTORY_SIZE;
    if (history_end == history_start)
        history_start = (history_start + 1) % HISTORY_SIZE;

    return 0;
}

int l_advance (lua_State* L)
{
    if (!lua_isinteger (L, 1))
    {
        ERROR("arg 1 of advance was not an integer");
        return 0;
    }

    lua_Integer i = lua_tointeger (L, 1);
    long int pred = ftell (file) + i;
    if (pred < 0 || file_length <= pred)
    {
        ERROR("arg 1 of advance would exceed file bounds");
        return 0;
    }

    fseek (file, i, SEEK_CUR);
    return 0;
}

int l_tell (lua_State* L)
{
    lua_pushinteger (L, ftell (file));
    return 1;
}

int l_hex (lua_State* L)
{
    if (!lua_isinteger (L, 1))
    {
        ERROR("arg 1 of hex was not an integer");
        return 0;
    }

    lua_Integer i = lua_tointeger (L, 1);
    if (i <= 0)
    {
        ERROR("arg 1 of hex must be strictly positive");
        return 0;
    }

    long int pos = ftell (file);
    if (file_length <= pos + i)
    {
        ERROR("arg 1 of hex would exceed file bounds");
        return 0;
    }

    int bufferpos = 0;
    bufferpos += sprintf (HISTORY, "\033[1;35m0x");
    for (lua_Integer j = 0; j < i; ++j)
    {
        bufferpos += sprintf (HISTORY, "%02X", fgetc (file));
    }
    bufferpos += sprintf (HISTORY, "\033[0m\n");
    history_filepos[history_end] = pos;

    history_end = (history_end + 1) % HISTORY_SIZE;
    if (history_end == history_start)
        history_start = (history_start + 1) % HISTORY_SIZE;

    fseek (file, pos, SEEK_SET);

    return 0;
}

int l_char (lua_State* L)
{
    if (!lua_isinteger (L, 1))
    {
        ERROR("arg 1 of char was not an integer");
        return 0;
    }

    lua_Integer i = lua_tointeger (L, 1);
    if (i <= 0)
    {
        ERROR("arg 1 of char must be strictly positive");
        return 0;
    }

    long int pos = ftell (file);
    if (file_length <= pos + i)
    {
        ERROR("arg 1 of char would exceed file bounds");
        return 0;
    }

    int bufferpos = 0;
    bufferpos += sprintf (HISTORY, "\033[1;32m");
    for (lua_Integer j = 0; j < i; ++j)
    {
        bufferpos += sprintf (HISTORY, "%c", fgetc (file));
    }
    bufferpos += sprintf (HISTORY, "\033[0m\n");
    history_filepos[history_end] = pos;

    history_end = (history_end + 1) % HISTORY_SIZE;
    if (history_end == history_start)
        history_start = (history_start + 1) % HISTORY_SIZE;

    fseek (file, pos, SEEK_SET);

    return 0;
}

int l_int (lua_State* L)
{
    if (!lua_isinteger (L, 1))
    {
        ERROR("arg 1 of int was not an integer");
        return 0;
    }

    lua_Integer i = lua_tointeger (L, 1);
    if (i <= 0)
    {
        ERROR("arg 1 of int must be strictly positive");
        return 0;
    }

    long int pos = ftell (file);
    if (file_length <= pos + i)
    {
        ERROR("arg 1 of int would exceed file bounds");
        return 0;
    }

    lua_Integer num = 0;
    for (int j = 0; j < i; ++j)
    {
        num <<= 8;
        num |= (fgetc(file)) & 0xFF;
    }
    fseek (file, pos, SEEK_SET);

    lua_pushinteger (L, num);
    return 1;
}



int main (int argc, char** argv)
{
    if (argc != 2)
    {
        printf ("unexpected number of args: %i\n", argc);
        return 1;
    }

    file = fopen (argv[1], "rb");
    if (!file)
    {
        printf ("could not open file %s\n", argv[1]);
        return 1;
    }

    fseek (file, 0, SEEK_END);
    file_length = ftell (file);
    fseek (file, 0, SEEK_SET);

    printf ("\033[?1049h");
    term = fileno (stdin);

    luaState = luaL_newstate ();
    luaL_openlibs (luaState);

    // register lua functions
    lua_pushcfunction (luaState, l_quit);    lua_setglobal (luaState, "quit");
    lua_pushcfunction (luaState, l_clear);   lua_setglobal (luaState, "clear");
    lua_pushcfunction (luaState, l_print);   lua_setglobal (luaState, "print");
    lua_pushcfunction (luaState, l_push);    lua_setglobal (luaState, "push");
    lua_pushcfunction (luaState, l_back);    lua_setglobal (luaState, "back");
    lua_pushcfunction (luaState, l_pop);     lua_setglobal (luaState, "pop");
    lua_pushcfunction (luaState, l_delete);  lua_setglobal (luaState, "delete");
    lua_pushcfunction (luaState, l_seek);    lua_setglobal (luaState, "seek");
    lua_pushcfunction (luaState, l_note);    lua_setglobal (luaState, "note");
    lua_pushcfunction (luaState, l_advance); lua_setglobal (luaState, "advance");
    lua_pushcfunction (luaState, l_tell);    lua_setglobal (luaState, "tell");
    lua_pushcfunction (luaState, l_hex);     lua_setglobal (luaState, "hex");
    lua_pushcfunction (luaState, l_char);    lua_setglobal (luaState, "char");
    lua_pushcfunction (luaState, l_int);     lua_setglobal (luaState, "int");

    while (running)
    {
        // clear screen
        printf ("\033[0m\033[2J\033[%i;1H", term_size.ws_row);

        // terminal size and sudo-title
        ioctl (term, TIOCGWINSZ, &term_size);
        printf ("\033[1;1H\033[1;48;2;70;95;75m %-*s \033[0m", term_size.ws_col - 2, argv[1]);

        // display history
        for (unsigned int i = history_start; i != history_end; i = (i + 1) % HISTORY_SIZE)
        {
            int row = term_size.ws_row - 1;
            row -= (history_end - i) % HISTORY_SIZE;

            if (row > 1)
                printf ("\033[%i;1H\033[48;2;60;60;60m 0x%08lX \033[0m %s",
                        row, history_filepos[i], history_log[i]);
        }

        // display report
        if (report_error) printf ("\033[1;31m");
        printf ("\033[%i;1H\033[48;2;60;60;60m %-*s\033[0m",
                term_size.ws_row, term_size.ws_col - 2, report);

        // display stack
        for (unsigned int i = 0; i < stack_elements; ++i)
        {
            int row = term_size.ws_row - 1;
            row -= stack_elements - i;

            if (row > 1)
                printf ("\033[%i;%iH\033[48;2;70;130;140m 0x%08lX \033[48;2;55;85;90m %-28s\033[0m",
                        row, term_size.ws_col - 40, stack[i], stack_notes[i]);
        }

        // color command bar
        printf ("\033[%i;%iH\033[1;48;2;100;150;110m%*s",
                term_size.ws_row - 1, 1, term_size.ws_col, "");

        // do command
        char command[1024] = {};
        printf ("\033[%i;%iH 0x%08lX >> ", term_size.ws_row - 1, 1, ftell (file));
        if (fgets (command, 1024, stdin))
        {
            report[0] = 0;
            report_error = 0;
            luaL_dostring (luaState, command);
        }
    }

    printf ("\033[?1049l");

    lua_close (luaState);
    fclose (file);

    return 0;
}
