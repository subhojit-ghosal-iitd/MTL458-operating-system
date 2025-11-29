#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glob.h>
#include <termios.h>
#include <dirent.h>
#include <ctype.h>
#define MAX_LINE_LENGTH 2048
#define MAX_ARGS 100
#define MAX_HISTORY 2048
#define PROMPT "MTL458 > "
#define ERROR_MESSAGE "Invalid Command\n"
char *g_history[MAX_HISTORY];
int g_history_count = 0;
char g_previous_dir[MAX_LINE_LENGTH] = "";
void readUserInput(char *buffer);
char **parseUserInput(char *buffer, int *arg_count);
void expand_WILD_cards(char ***args_ptr, int *arg_count);
int executeSubCommand(char *command_string);
void executeSimpleCommand(char **args, int arg_count);
void executePipedCommand(char **args1, int arg_count1, char **args2, int arg_count2);
void executeRedirectedCommand(char **args, int arg_count, char *filepath, int open_flags, int stream_fileno);
int handleBuiltinCommand(char **args, int arg_count);
void changeDirectory(char *target_dir);
void displayHistory(int num_commands);
void addToHistory(const char *command_line);
void processLine(char *line);
int findOperator(char **args, int arg_count, const char *operator);
char *trimWhitespace(char *str);
void enable_Raw_MODE(struct termios *original_termios);
void disableRawMode(const struct termios *original_termios);
void handle_Tab_Completion(char *line_buffer, int *cursor_pos);
// void executePipedCommand(char **args1, int arg_count1, char **args2, int arg_count2);
// void executeRedirectedCommand(char **args, int arg_count, char *filepath, int open_flags, int stream_fileno);
// int handleBuiltinCommand(char **args, int arg_count);
// void changeDirectory(char *target_dir);


int main()
{
    char line_buffer[MAX_LINE_LENGTH];

    // Here the code Store the starting directory to enable `cd -` later
    if (getcwd(g_previous_dir, sizeof(g_previous_dir)) == NULL)
    {
        perror("getcwd: error getting initial directory");
    }

    // read, parse, execute :
    while (1)
    {
        printf("%s", PROMPT);
        fflush(stdout);

        readUserInput(line_buffer);

        if (strlen(line_buffer) == 0)
        {
            continue;
        }

        addToHistory(line_buffer);
        processLine(line_buffer);
    }

    // addToHistory(line_buffer);
    // processLine(line_buffer);

    return 0;
}

// _________________________________________________________________________________

// Terminal Handling

// We Switch terminal to raw mode
// to read single characters (for tab-completion)
void enable_Raw_MODE(struct termios *original_termios)
{
    tcgetattr(STDIN_FILENO, original_termios);
    struct termios raw = *original_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    // I Turn off echo and canonical mode

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode(const struct termios *original_termios)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, original_termios);
}

//  Input Handling
void readUserInput(char *buffer)
{

    struct termios original_termios;
    enable_Raw_MODE(&original_termios);

    int cursor_pos = 0;
    char c;
    memset(buffer, 0, MAX_LINE_LENGTH);

    while (read(STDIN_FILENO, &c, 1) == 1)
    {

        if (c == '\n')
        {

            // Enter
            printf("\n");
            break;
        }
        else if (c == 127 || c == 8)
        {

            // Backspace

            if (cursor_pos > 0)
            {
                cursor_pos--;
                printf("\b \b");
                // Erase
                fflush(stdout);
            }
        }
        else if (c == '\t')
        {

            // Tab

            handle_Tab_Completion(buffer, &cursor_pos);
        }
        else if (c >= 32 && c < 127)
        {
            // Any other printable character

            if (cursor_pos < MAX_LINE_LENGTH - 1)
            {
                buffer[cursor_pos++] = c;
                printf("%c", c);
                // Manually echo the character

                fflush(stdout);
            }
        }
    }

    buffer[cursor_pos] = '\0';

    disableRawMode(&original_termios);
}


void handle_Tab_Completion(char *line_buffer, int *cursor_pos)
{

    int start = *cursor_pos - 1;
    while (start >= 0 && line_buffer[start] != ' ' && line_buffer[start] != '/')
    {
        start--;
    }
    start++;


    // while (start >= 0 && line_buffer[start] != ' ' && line_buffer[start] != '/')
    // {
    //     start--;
    // }
    // start++;



    char pattern[MAX_LINE_LENGTH];
    strncpy(pattern, &line_buffer[start], *cursor_pos - start);
    pattern[*cursor_pos - start] = '\0';
    strcat(pattern, "*");

    glob_t glob_results;
    if (glob(pattern, GLOB_TILDE, NULL, &glob_results) == 0)
    {
        // If there's exactly one match, complete it
        if (glob_results.gl_pathc == 1)
        {
            char *match = glob_results.gl_pathv[0];
            int match_len = strlen(match);

            // Erase the partially typed word from the screen
            for (int i = start; i < *cursor_pos; i++)
            {
                printf("\b \b");
            }

            // Update the buffer and cursor position
            strcpy(&line_buffer[start], match);
            *cursor_pos = start + match_len;

            // Print the completed word
            printf("%s", match);
            fflush(stdout);
        }
    }
    globfree(&glob_results);
}

// History Management
// Adds a command to the history array
void addToHistory(const char *command_line)
{

    // Don't add consecutive duplicate commands
    if (g_history_count > 0 && strcmp(g_history[g_history_count - 1], command_line) == 0)
    {
        return;
    }

    if (g_history_count < MAX_HISTORY)
    {

        g_history[g_history_count++] = strdup(command_line);
    }
    // if (g_history_count > 0 && strcmp(g_history[g_history_count - 1], command_line) == 0)
    // {
    //     return;
    // }

    // if (g_history_count < MAX_HISTORY)
    // {

    //     g_history[g_history_count++] = strdup(command_line);
    // }
    else
    {

        // If history is full, discard the oldest command
        free(g_history[0]);
        for (int i = 1; i < MAX_HISTORY; i++)
        {
            g_history[i - 1] = g_history[i];
        }
        g_history[MAX_HISTORY - 1] = strdup(command_line);
    }
}

// Command Parsing & Execution

// Splits a command string into an array of arguments, handling quotes and escapes.
char **parseUserInput(char *buffer, int *arg_count)
{
    static char *args[MAX_ARGS];
    *arg_count = 0;
    char *read_ptr = buffer;

    while (*read_ptr && *arg_count < MAX_ARGS - 1)
    {
        // We have to skip leading whitespace
        while (*read_ptr && isspace((unsigned char)*read_ptr))
        {
            read_ptr++;
        }
        if (*read_ptr == '\0')
            break;

        args[*arg_count] = read_ptr;
        (*arg_count)++;

        char *write_ptr = read_ptr;

        if (*read_ptr == '"')
        {
            read_ptr++; 
            
            args[*arg_count - 1] = write_ptr;

            while (*read_ptr && *read_ptr != '"')
            {
                if (*read_ptr == '\\' && *(read_ptr + 1) != '\0')
                {
                    read_ptr++; 
                    
                    // We skip backslash

                    switch (*read_ptr)
                    {
                    case 'n':
                        *write_ptr++ = '\n';
                        break;
                    case 't':
                        *write_ptr++ = '\t';
                        break;
                    case '"':
                        *write_ptr++ = '"';
                        break;
                    case '\\':
                        *write_ptr++ = '\\';
                        break;
                    default:
                        *write_ptr++ = *read_ptr;
                        break;
                    }
                }
                else
                {
                    *write_ptr++ = *read_ptr;

                    // switch (*read_ptr)
                    // {
                    // case 'n':
                    //     *write_ptr++ = '\n';
                    //     break;
                    // case 't':
                    //     *write_ptr++ = '\t';
                    //     break;
                    
                    // case '"':
                    //     *write_ptr++ = '"';
                    
                    
                    //     break;
                    // case '\\':
                    //     *write_ptr++ = '\\';
                    
                    //     break;
                    // default:
                    //     *write_ptr++ = *read_ptr;
                    //     break;
                    // }


                }
                read_ptr++;
            }
            if (*read_ptr == '"')
            {
                read_ptr++; 
                // We skip closing quote
            }
        }
        else
        {
            // Handle unquoted argument
            while (*read_ptr && !isspace((unsigned char)*read_ptr))
            {
                *write_ptr++ = *read_ptr++;
            }
        }

        if (*read_ptr)
        {
            *read_ptr++ = '\0';
        }
        *write_ptr = '\0'; 
        
        // Terminate the argument

    }
    args[*arg_count] = NULL;
    return args;
}

// Expands wildcard characters (*) in command arguments
void expand_WILD_cards(char ***args_ptr, int *arg_count)
{
    int has_wildcard = 0;
    for (int i = 1; i < *arg_count; i++)
    {
        if (strchr((*args_ptr)[i], '*') != NULL)
        {
            has_wildcard = 1;
            break;
        }
    }
    if (!has_wildcard)
        return;

    static char *expanded_args[MAX_ARGS];
    int expanded_arg_count = 0;
    glob_t glob_results;

    expanded_args[expanded_arg_count++] = (*args_ptr)[0]; 
    // Preserve command name

    for (int i = 1; i < *arg_count; i++)
    {
        if (strchr((*args_ptr)[i], '*') != NULL)
        {
            if (glob((*args_ptr)[i], 0, NULL, &glob_results) == 0 && glob_results.gl_pathc > 0)
            {
                for (size_t j = 0; j < glob_results.gl_pathc; j++)
                {
                    if (expanded_arg_count < MAX_ARGS - 1)
                    {
                        expanded_args[expanded_arg_count++] = glob_results.gl_pathv[j];
                    }
                }

                // for (size_t j = 0; j < glob_results.gl_pathc; j++)
                // {
                //     if (expanded_arg_count < MAX_ARGS - 1)
                //     {
                //         expanded_args[expanded_arg_count++] = glob_results.gl_pathv[j];
                //     }
                // }
            }
            else
            {
                if (expanded_arg_count < MAX_ARGS - 1)
                {
                    expanded_args[expanded_arg_count++] = (*args_ptr)[i];
                }
            }
        }
        else
        {
            if (expanded_arg_count < MAX_ARGS - 1)
            {
                expanded_args[expanded_arg_count++] = (*args_ptr)[i];
            }
        }
    }

    expanded_args[expanded_arg_count] = NULL;
    *args_ptr = expanded_args;
    *arg_count = expanded_arg_count;
}

// Executes a basic command in a new process
void executeSimpleCommand(char **args, int arg_count)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
    }
    else if (pid == 0)
    { 
        // Child process
        expand_WILD_cards(&args, &arg_count);
        if (execvp(args[0], args) == -1)
        {
            fprintf(stderr, ERROR_MESSAGE);
            exit(EXIT_FAILURE);
        }
    }
    else
    { 
        // Parent process
        wait(NULL);
    }
}

// Executes a piped command (e.g., ls | grep)
void executePipedCommand(char **args1, int arg_count1, char **args2, int arg_count2)
{
    int pipe_fds[2];
    pid_t pid1, pid2;

    if (pipe(pipe_fds) < 0)
    {
        perror("pipe");
        return;
    }

    pid1 = fork();
    if (pid1 < 0)
    {
        perror("fork");
        return;
    }

    if (pid1 == 0)
    {                                    
        
        close(pipe_fds[0]);               
        
        dup2(pipe_fds[1], STDOUT_FILENO); 

        close(pipe_fds[1]);

        expand_WILD_cards(&args1, &arg_count1);
        if (execvp(args1[0], args1) < 0)
        {
            fprintf(stderr, ERROR_MESSAGE);
            exit(EXIT_FAILURE);
        }

        // close(pipe_fds[0]);               
        
        // dup2(pipe_fds[1], STDOUT_FILENO); 

        // close(pipe_fds[1]);

        // expand_WILD_cards(&args1, &arg_count1);
        // if (execvp(args1[0], args1) < 0)
        // {
        //     fprintf(stderr, ERROR_MESSAGE);
        //     exit(EXIT_FAILURE);
        // }
    }

    pid2 = fork();
    if (pid2 < 0)
    {
        perror("fork");
        return;
    }

    if (pid2 == 0)
    {
        close(pipe_fds[1]);
        dup2(pipe_fds[0], STDIN_FILENO);
        close(pipe_fds[0]);

        expand_WILD_cards(&args2, &arg_count2);
        if (execvp(args2[0], args2) < 0)
        {
            fprintf(stderr, ERROR_MESSAGE);
            exit(EXIT_FAILURE);
        }
    }

    // Parent process closes both ends of the pipe and waits for children
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

// Executes a command with its input or output redirected to a file
void executeRedirectedCommand(char **args, int arg_count, char *filepath, int open_flags, int stream_fileno)
{

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return;
    }

    if (pid == 0)
    {
        // Child process

        expand_WILD_cards(&args, &arg_count);

        int fd = open(filepath, open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd < 0)
        {
            perror("open");
            exit(EXIT_FAILURE);
        }

        dup2(fd, stream_fileno);
        // Redirect stdin or stdout
        close(fd);

        if (execvp(args[0], args) < 0)
        {
            fprintf(stderr, ERROR_MESSAGE);
            exit(EXIT_FAILURE);
        }
    }
    else
    {

        // Parent process
        wait(NULL);
    }
}

//  Built-in Commands

// Checks for and handles built-in commands like cd, exit, history
int handleBuiltinCommand(char **args, int arg_count)
{
    if (strcmp(args[0], "exit") == 0)
    {
        exit(0);
    }

    if (strcmp(args[0], "cd") == 0)
    {

        char *target_dir = NULL;

        if (arg_count == 1)
        {

            target_dir = getenv("HOME");
        }
        else
        {

            char *path_argument = args[1];

            if (strcmp(path_argument, "~") == 0)
            {

                target_dir = getenv("HOME");
            }
            else if (strcmp(path_argument, "-") == 0)
            {

                if (g_previous_dir[0] == '\0')
                {
                    fprintf(stderr, "cd: OLDPWD not set\n");
                    return 1;
                }

                target_dir = g_previous_dir;
                printf("%s\n", target_dir);
            }
            else if (strchr(path_argument, '*') != NULL)
            {

                glob_t glob_results;

                if (glob(path_argument, 0, NULL, &glob_results) == 0)
                {

                    if (glob_results.gl_pathc == 1)
                    {

                        target_dir = glob_results.gl_pathv[0];
                    }
                    else if (glob_results.gl_pathc > 1)
                    {

                        fprintf(stderr, "%s", ERROR_MESSAGE);
                        globfree(&glob_results);

                        return 1;
                    }
                    else
                    {

                        target_dir = path_argument;
                    }

                    if (target_dir)
                        changeDirectory(target_dir);
                    globfree(&glob_results);
                }
                else
                {

                    changeDirectory(path_argument);
                }
                return 1;
            }
            else
            {

                target_dir = path_argument;
            }
        }

        if (target_dir)
        {
            changeDirectory(target_dir);
        }

        return 1;
    }

    if (strcmp(args[0], "history") == 0)
    {
        int num_commands = g_history_count;
        if (arg_count > 1)
        {
            num_commands = atoi(args[1]);
        }
        displayHistory(num_commands);
        return 1;
    }
    return 0; // Not a built-in command
}

// Changes the current working directory
void changeDirectory(char *target_dir)
{
    char current_dir[MAX_LINE_LENGTH];

    if (getcwd(current_dir, sizeof(current_dir)) == NULL)
    {
        perror("getcwd");
        return;
    }

    if (chdir(target_dir) != 0)
    {
        perror("cd");
    }
    else
    {

        strcpy(g_previous_dir, current_dir);
        // Update previous directory
    }
}

// Displays the last N commands from history
void displayHistory(int num_commands)
{
    int start = (g_history_count > num_commands) ? (g_history_count - num_commands) : 0;
    for (int i = start; i < g_history_count; i++)
    {
        printf("%s\n", g_history[i]);
    }
}

// Top-Level Logic

// We find the position of an operator
// (like | or <) in the argument list
int findOperator(char **args, int arg_count, const char *operator)
{

    for (int i = 0; i < arg_count; i++)
    {
        if (strcmp(args[i], operator) == 0)
        {
            return i;
        }
    }

    return -1;
    // Not found
}

// We removes leading/trailing whitespace from a string (for command separators)
char *trimWhitespace(char *str)
{
    char *end;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    end[1] = '\0';

    return str;
}

// Processes a full line of input
// handling ; and && separators
void processLine(char *line)
{

    char *line_saveptr;
    char *semicolon_part = strtok_r(line, ";", &line_saveptr);

    while (semicolon_part)
    {

        char *current_pos = semicolon_part;
        int previous_command_succeeded = 1;

        while (current_pos && *current_pos && previous_command_succeeded)
        {

            char *and_separator = strstr(current_pos, "&&");
            char *sub_command;

            if (and_separator)
            {

                *and_separator = '\0';
                sub_command = current_pos;
                current_pos = and_separator + 2;
            }
            else
            {

                sub_command = current_pos;
                current_pos = NULL;
            }

            sub_command = trimWhitespace(sub_command);

            if (strlen(sub_command) > 0)
            {

                int status = executeSubCommand(sub_command);
                if (status != 0)
                {
                    previous_command_succeeded = 0;
                }
            }
        }
        semicolon_part = strtok_r(NULL, ";", &line_saveptr);
    }
}

// Executes a single command
// may have pipes or redirection
int executeSubCommand(char *command_string)
{

    int arg_count;
    char *command_copy = strdup(command_string);
    char **args = parseUserInput(command_copy, &arg_count);
    int status = 0;

    if (arg_count == 0)
    {
        free(command_copy);
        return 0;
    }

    if (handleBuiltinCommand(args, arg_count))
    {
        free(command_copy);
        return 0;
    }

    int pipe_pos = findOperator(args, arg_count, "|");
    int in_redir_pos = findOperator(args, arg_count, "<");
    int out_redir_pos = findOperator(args, arg_count, ">");
    int out_append_pos = findOperator(args, arg_count, ">>");

    if (pipe_pos != -1)
    {

        args[pipe_pos] = NULL;
        executePipedCommand(args, pipe_pos, &args[pipe_pos + 1], arg_count - pipe_pos - 1);
    }
    else if (in_redir_pos != -1)
    {

        char *filepath = args[in_redir_pos + 1];
        args[in_redir_pos] = NULL;

        executeRedirectedCommand(args, in_redir_pos, filepath, O_RDONLY, STDIN_FILENO);
    }
    else if (out_redir_pos != -1)
    {

        char *filepath = args[out_redir_pos + 1];
        args[out_redir_pos] = NULL;

        executeRedirectedCommand(args, out_redir_pos, filepath, O_WRONLY | O_CREAT | O_TRUNC, STDOUT_FILENO);
    }
    else if (out_append_pos != -1)
    {

        char *filepath = args[out_append_pos + 1];
        args[out_append_pos] = NULL;

        executeRedirectedCommand(args, out_append_pos, filepath, O_WRONLY | O_CREAT | O_APPEND, STDOUT_FILENO);
    }
    else
    {

        pid_t pid = fork();

        if (pid < 0)
        {

            perror("fork");
            status = -1;
        }
        else if (pid == 0)
        {

            // Child process

            expand_WILD_cards(&args, &arg_count);

            if (execvp(args[0], args) == -1)
            {

                fprintf(stderr, ERROR_MESSAGE);
                exit(1);
            }
        }
        else
        {

            // Parent process

            int child_status;
            waitpid(pid, &child_status, 0);

            if (WIFEXITED(child_status))
            {

                status = WEXITSTATUS(child_status);
            }
            else
            {

                status = -1;
            }
        }
    }

    free(command_copy);
    return status;
}
