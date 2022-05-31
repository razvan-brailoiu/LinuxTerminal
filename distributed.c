#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <ctype.h>
#define BLOCK_SIZE 512
// OS distributed assignment: made by Razvan Brailoiu

// defined a block size in order to easier read big chunks of data
// customized my own help command
void help_command()
{
    puts(" \n ***HELP PAGE - Razvan Brailoiu's Shell Simulation****"
         "\n List of commands supported as of now: "
         "\n 1. help (you already used this) "
         "\n 2. exit (exits the shell)"
         "\n 3. cd <dir> changes the current directory"
         "\n 4. pwd prints the current directory"
         "\n 5. type <entry> displays the type of the given entry - reg. file, symbolic link ,directory"
         "\n 6. create <type> <entry> <target> <dir>, type: -f, -d, -l for file, dir, symlink later"
         "\n 6 -continued: in case dir is provided and valid, the entry will be created in that directory"
         "\n 7. run <command> <args>. runs any valid linux command with arguments"
         "\n 8. extend the run command with pipe: | ");
}

// function for pwd
void print_workingdir()
{
    char currentWokingDir[300];
    getcwd(currentWokingDir, sizeof(currentWokingDir));
    printf("%s", currentWokingDir);
}

// function for always simulating a starting statement like a real terminal
void print_start()
{
    char *username;
    username = getenv("USER");
    printf("%s @device -", username);
    print_workingdir();
    printf("  ");
}

// function for changing directory using chdir sys call
void change_dir(const char *mypath)
{
    if (chdir(mypath) != 0)
    {
        printf("Error in changing directory. Invalid path");
        return;
    }
}

// function to recognize and print the type of an entry
// we are using struct stat
void printType(char *entry)
{
    struct stat buf;
    if (lstat(entry, &buf) == -1)
    {
        printf("This file does not exist or cannot be opened");
    }
    else
    {
        if (S_ISREG(buf.st_mode))
        {
            printf(" regular file");
        }
        else if (S_ISDIR(buf.st_mode))
        {
            printf(" directory");
        }
        else if (S_ISLNK(buf.st_mode))
        {
            printf(" symbolic link");
        }
        else
        {
            printf("Entry type not mentioned\n");
        }
    }
}

// function for a simple run <command> [<args>]
// one child, fork
// it returns an int (the status) so we cand then use that with status command
int forkit(char *command, char **list)
{
    pid_t pid;
    int i = 0;
    // first, we right shift the array
    for (i = 0; list[i]; i++)
    {
        list[i] = list[i + 1];
    }
    if ((pid = fork()) < 0)
    {
        printf("error in spawning child");
    }
    else if (pid == 0)
    {
        // printf("\n in child");
        int status_code = execvp(command, list);
        if (status_code < -1)
        {
            printf("Abnormally executed");
        }
        exit(127);
    }
    else
    {
        int returnValue;
        if (waitpid(pid, &returnValue, 0) == -1)
        {
            printf("error in waitpid");
        }
        else
        {
            return returnValue;
        }
    }
    return -1;
}

// function to execute pipe, it first breaks the whole command in 2 char**: one before pipe, one after pipe
// then it creates two children processes to execute the tasks
// the first one writes at the write end of the pipe
// the second one reads from the read end exactly what the other one wrote
// it also returns and int - status
int execute_pipe(char **commands)
{
    char **parsed_one;
    char **parsed_after_pipe;
    parsed_one = malloc(sizeof(char *) * BLOCK_SIZE);
    parsed_after_pipe = malloc(sizeof(char *) * BLOCK_SIZE);
    for (int i = 0; i < 40; i++)
    {
        parsed_one[i] = malloc(sizeof(char *) * BLOCK_SIZE);
        parsed_after_pipe[i] = malloc(sizeof(char *) * BLOCK_SIZE);
    }

    int k = 0;
    int it = 1;
    while (strcmp(commands[it], "|"))
    {
        strcpy(parsed_one[k], commands[it]);
        k++;
        it++;
    }
    parsed_one[k] = NULL;
    it += 1;
    int m = 0;
    while (commands[it] != NULL)
    {
        strcpy(parsed_after_pipe[m], commands[it]);
        m++;
        it++;
    }
    parsed_after_pipe[m] = NULL;

    int pipefd[2];
    pid_t p1, p2;

    if (pipe(pipefd) < 0)
    {
        printf("\nError in creating pipe");
        return -1;
    }
    if ((p1 = fork()) < 0)
    {
        printf("\nError in spawning child one");
        return -1;
    }
    if (p1 == 0)
    {
        close(STDOUT_FILENO);           // closing the other end
        dup2(pipefd[1], STDOUT_FILENO); // redirects stdout into the pipe, old was dup(pipedf[1])
        close(pipefd[0]);               // close reading end
        close(pipefd[1]);               // close both

        int status_code = execvp(parsed_one[0], parsed_one);
        if (status_code < 0)
        {
            printf("Abnormally executed");
            exit(0);
        }
        // exit(127);
    }
    else
    {
        // parent code
        if ((p2 = fork()) < 0)
        {
            printf("\nError in spawning child two");
            return -1;
        }
        else if (p2 == 0)
        {
            close(STDIN_FILENO);
            dup2(pipefd[0], STDIN_FILENO); // redirects stdin into the pipe   old was dup(pipefd[0])
            close(pipefd[1]);              // close write end
            close(pipefd[0]);              // close reading end
            int status_code = execvp(parsed_after_pipe[0], parsed_after_pipe);
            if (status_code < 0)
            {
                printf("Abnormally executed");
                exit(0);
            }
        }
        else
        {
            int retvalueone, retvaluetwo;
            close(pipefd[0]);
            close(pipefd[1]);
            // now we wait for the children to finish execution and check exit status
            if (waitpid(p1, &retvalueone, 0) == -1 || waitpid(p2, &retvaluetwo, 0) == -1)
            {
                printf("\nWaitpid not ok executed");
            }
            else
            {
                return retvalueone + retvaluetwo;
            }
        }
    }

    return -1;
}

// classic function to break the whole input line into chunks (words) for easier execution after
char **command(char *input_line)
{
    char *p;
    char **final_string;
    final_string = malloc(sizeof(char **));
    p = strtok(input_line, " ");
    int k = 0;

    while (p != NULL)
    {
        final_string[k] = (char *)malloc(BLOCK_SIZE);
        strcpy(final_string[k], p);
        p = strtok(NULL, " ");
        k++;
    }
    final_string[k] = NULL;
    return final_string;
}

int main()
{
    char buf[BLOCK_SIZE];
    char *check_return;
    char **command_list;
    mode_t mode = 0;
    int status = 0;

    while (1)
    {
        printf("\n");
        print_start();
        char c;
        if (!isspace(c = getchar())) // this helps us to avoid segmentation fault on empty command
        {
            ungetc(c, stdin);
        }
        else
            continue;

        check_return = fgets(buf, BLOCK_SIZE, stdin);
        if (check_return)
        {
            buf[strcspn(buf, "\n")] = '\0'; // we make it null-terminated
            command_list = command(buf);

            // now, we test for each command in a long if-else if
            if (strcmp(command_list[0], "help") == 0)
            {
                if (command_list[1] == NULL)
                {
                    help_command();
                }
                else
                {
                    printf("\n help is executed solely, no more args\n");
                }
            }
            else if (strcmp(command_list[0], "exit") == 0)
            {
                if (command_list[1] == NULL)
                {
                    printf("\nGoodbye\n");
                    exit(0);
                }
                else
                {
                    printf("\n exit is executed solely, no more args\n");
                }
            }
            else if (strcmp(command_list[0], "cd") == 0)
            {
                change_dir(command_list[1]);
            }
            else if (strcmp(command_list[0], "pwd") == 0)
            {
                if (command_list[1] == NULL)
                {
                    print_workingdir();
                }
                else
                {
                    printf("\n pwd is executed solely, no more args\n");
                }
            }
            else if (strcmp(command_list[0], "type") == 0)
            {
                if (command_list[2] == NULL)
                {
                    printType(command_list[1]);
                }
                else
                {
                    printf("\n no more arguments after filename\n");
                }
            }
            else if (strcmp(command_list[0], "create") == 0)
            {

                if (strcmp(command_list[1], "-f") == 0)
                {
                    if (command_list[2] && command_list[3])
                    {
                        // case create -f file <dir>
                        char currentWokingDir[200];
                        getcwd(currentWokingDir, sizeof(currentWokingDir));

                        if (chdir(command_list[3]) == 0)
                        {
                            if (creat(command_list[2], mode) < 0)
                                printf("error in creating file");
                            else
                                printf("directory created successfully in %s", command_list[3]);
                        }
                        else
                        {
                            printf("\nError in creating the file on another path, maybe it already exists?");
                        }

                        if (chdir(currentWokingDir) != 0)
                        {
                            printf("Error returning to the initial directory");
                        }
                    }
                    else if (command_list[2] && !command_list[3])
                    {
                        // case create -f file
                        if (creat(command_list[2], mode) < 0)
                            printf("error in creating file, maybe it already exists?");
                        else
                            printf("file created successfully");
                    }
                    else
                    {
                        printf("Null file name");
                    }
                }
                else if (strcmp(command_list[1], "-d") == 0)
                {
                    if (command_list[2] && command_list[3])
                    {
                        // case create -d dir <another_dir>
                        char currentWokingDir[200];
                        getcwd(currentWokingDir, sizeof(currentWokingDir));

                        if (chdir(command_list[3]) == 0)
                        {
                            if (mkdir(command_list[2], mode) < 0)
                                printf("error in creating directory, maybe it already exists?");
                            else
                                printf("directory created successfully in %s", command_list[3]);
                        }
                        else
                        {
                            printf("\nError in creating the directory on another path, maybe it already exists?");
                        }

                        if (chdir(currentWokingDir) != 0)
                        {
                            printf("Error returning to the initial directory");
                        }
                    }
                    else if (command_list[2] && !command_list[3])
                    {
                        // case create -d dir
                        if (mkdir(command_list[2], mode) < 0)
                            printf("error in creating directory, maybe it already exists?");
                        else
                            printf("directory created successfully");
                    }
                    else
                    {
                        printf("Null dir name");
                    }
                }
                else if (strcmp(command_list[1], "-l") == 0)
                {
                    if (command_list[2] && command_list[3] && command_list[4])
                    {
                        // case create -l name target dir
                        char currentWokingDir[200];
                        getcwd(currentWokingDir, sizeof(currentWokingDir));

                        if (chdir(command_list[4]) == 0)
                        {
                            if (symlink(command_list[2], command_list[3]) < 0)
                                printf("error in creating symbolic link, maybe it already exists?");
                            else
                                printf("symbolic link created successfully in %s", command_list[4]);
                        }
                        else
                        {
                            printf("\nError in creating the link on another path");
                        }

                        if (chdir(currentWokingDir) != 0)
                        {
                            printf("Error returning to the initial directory");
                        }
                    }
                    else if (command_list[2] && command_list[3] && !command_list[4])
                    {
                        // case create -l name target
                        if (symlink(command_list[2], command_list[3]) < 0)
                            printf("error in creating symbolic link, maybe it already exists?");
                        else
                            printf("symbolic link created successfully");
                    }
                    else
                    {
                        printf("Wrong syntax");
                    }
                }
                else
                {
                    printf("Unknown argument for create");
                }
            }
            else if (strcmp(command_list[0], "run") == 0)
            {
                // run can either be used with pipe or no pipe
                // we check for an existing pipe
                int pipe_flag = 0; // no pipe initially
                int k = 0;
                while (command_list[k] != NULL)
                {
                    if (strcmp(command_list[k], "|") == 0)
                    {
                        pipe_flag = 1;
                    }
                    k++;
                }
                if (pipe_flag)
                {
                    // we are in the case where pipe exists, so execute accordingly
                    // printf("executing pipe");
                    execute_pipe(command_list);
                }
                else
                {
                    // no pipe, simple run
                    status = forkit(command_list[1], command_list);
                }
            }
            else if (strcmp(command_list[0], "status") == 0)
            {
                if (command_list[1] == NULL)
                {
                    printf("%d", status);
                    status = 0;
                }
                else
                {
                    printf("\n status must be executed solely, no more args\n");
                }
            }
            else
            {
                printf("unknown command\n");
            }
        }
    }
}