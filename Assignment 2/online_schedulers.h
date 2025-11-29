#pragma once

//Can include any other headers as needed
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

#include <stdint.h>


typedef struct {
    char *command;
    bool finished;
    bool error;    
    uint64_t start_time;
    uint64_t completion_time;
    uint64_t turnaround_time;
    uint64_t waiting_time;
    uint64_t response_time;
    bool started; 
    int process_id;

    // my parameters
    uint64_t arival_time ;
    uint64_t burst_time ;
    uint64_t total_burst_time ;
    double avg_burst ;
    int run_count ;
    bool done ;

} Process;

// Function prototypes
void ShortestJobFirst(int k);
void MultiLevelFeedbackQueue(int quantum0_, int quantum1_, int quantum2_, int boostTime_);

/*
---- Taking Input ----
Normally, reading from stdin (e.g. using fgets() or read()) will block — i.e. pause your program — until the user types something and presses Enter.
use fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK)
used to make standard input (stdin) non-blocking
your program can check for user input without stopping execution.

*/

const int MAX_COMMAND_LEN = 3000 ;
const int MAX_PROCESS = 70 ;
const int INITIAL_AVG_BURST = 1000.0 ; // in ms
const int START_PRIORITY = 1 ; // Medium priority level
uint64_t start_time_global ;

struct timeval tv ;
uint64_t current_time_ms() {
    gettimeofday(&tv , NULL) ;
    return tv.tv_sec * 1000ULL + tv.tv_usec / 1000 ;
}

void input_process_SJF(Process* p , int *total_process , int *total_unique_process , u_int64_t** burst_history , int k) {
    char *line = NULL;
    size_t linecap = 0;
    ssize_t nread;

    bool x = 0 ; // debug variable

    while((nread = getline(&line, &linecap, stdin)) != -1) {

        x=1 ;

        if(nread > 0 && line[nread - 1] == '\n') {
            line[nread - 1] = '\0';
            nread--;
        }

        /* Skip empty lines (optional) */
        if(nread == 0) continue;

        int idx = *total_process;
        *total_process = idx+1;

        p[idx].arival_time = current_time_ms() - start_time_global ;

        p[idx].command = (char *)malloc(MAX_COMMAND_LEN);
        if(!p[idx].command) {
            p[idx].error = 1;
            p[idx].done = 1;
            continue;
        }

        strncpy(p[idx].command, line, MAX_COMMAND_LEN - 1);
        p[idx].command[MAX_COMMAND_LEN - 1] = '\0';

        p[idx].finished = 0;
        p[idx].error = 0;
        p[idx].started = 0;
        p[idx].burst_time = 0;
        p[idx].total_burst_time = 0;
        p[idx].avg_burst = INITIAL_AVG_BURST;
        p[idx].run_count = 0;
        p[idx].done = 0;
        p[idx].process_id = 0;

        // iterate on all processess and check if this process is already there
        int I = -1 ;
        for(int i = idx-1 ; i >= 0 ; i--) {
            if(strcmp(p[i].command, p[idx].command) == 0) {
                I = i ;
            }
        }

        if(I != -1) {
            uint64_t sum = 0 ;
            int cnt = 0 ;
            for(int j = 0 ; j < k ; j++) {
                if(burst_history[I][j] != 0) {
                    sum += burst_history[I][j] ;
                    cnt++ ;
                }
            }
            p[idx].avg_burst = (cnt == 0) ? INITIAL_AVG_BURST : (double) ((double)sum / (double)cnt) ;
        }


        pid_t pid = fork();

        if (pid < 0) {

            p[idx].error = 1;
            p[idx].done = 1;
            free(p[idx].command);
            p[idx].command = NULL;
        } else if (pid == 0) {
            char *cmd = strdup(p[idx].command ? p[idx].command : "");
            if (!cmd) _exit(1);

            char *saveptr;
            char *token = strtok_r(cmd, " ", &saveptr);

            char *args[MAX_COMMAND_LEN];
            int j = 0;
            while (token && j < MAX_COMMAND_LEN - 1) {
                args[j++] = token;
                token = strtok_r(NULL, " ", &saveptr);
            }
            args[j] = NULL;

            // printf("execvp kiya: %s\n", p[idx].command); // debug line

            execvp(args[0], args);
            free(cmd);
            _exit(1);
        } else {
            kill(pid, SIGSTOP);
            p[idx].process_id = pid;
        }

        // printf("New Process: %s | PID: %d | Avg Burst: %.2f\n", p[idx].command, p[idx].process_id, p[idx].avg_burst); // debug line
        
    }

    free(line);
}

int get_process_SJF(Process* p , int total_process) {
    int idx = -1 ;
    double min_avg_burst = 1e18 ;
    for(int i = 0 ; i < total_process ; i++) {
        if(p[i].done) continue ;
        if(p[i].avg_burst < min_avg_burst) {
            min_avg_burst = p[i].avg_burst ;
            idx = i ;
        }
    }
    return idx ;
}

void ShortestJobFirst(int k) {
    Process process_info[MAX_PROCESS] ;
    
    // u_int64_t burst_history[MAX_PROCESS][k] ;
    u_int64_t** burst_history = (u_int64_t**)malloc(MAX_PROCESS * sizeof(u_int64_t*));
    for(int i = 0; i < MAX_PROCESS; i++) {
        burst_history[i] = (u_int64_t*)malloc(k * sizeof(u_int64_t));
        
        for(int j = 0 ; j < k ; j++) {
            burst_history[i][j] = 0 ;
        }

    }

    int total_process = 0 ;
    int done_cnt = 0 ;
    int total_unique_process = 0 ;

    FILE *csv = fopen("result_online_SJF.csv", "w");
    fclose(csv);
    start_time_global = current_time_ms();
    
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    
    while(1) {
        
        // check for input
        input_process_SJF(process_info , &total_process , &total_unique_process , burst_history , k) ;

        int p_idx = get_process_SJF(process_info , total_process) ;
        if(p_idx == -1) continue ; // no process found

        
        
        process_info[p_idx].start_time = current_time_ms() - start_time_global ;
        process_info[p_idx].response_time = current_time_ms() - start_time_global - process_info[p_idx].arival_time ;
        process_info[p_idx].started = 1 ;
        
        uint64_t context_start_time = current_time_ms() - start_time_global ;
        
        
        // resume
        kill(process_info[p_idx].process_id , SIGCONT) ;
        
        
        // wait till the process ends
        int status ; // Declares a variable to hold exit information of the child.
        waitpid(process_info[p_idx].process_id , &status , 0) ;
        
        uint64_t context_finish_time = current_time_ms() - start_time_global ;
        
        // insert burst time in history
        
        
        
        
        int I = -1 ;
        for(int i = p_idx-1 ; i >= 0 ; i--) {
            if(i == p_idx) continue; // skip self
            if(strcmp(process_info[i].command, process_info[p_idx].command) == 0) {
                I = i ;
            }
        }
        
        // printf("context => p_idx: %d , command: %s\n", p_idx, process_info[p_idx].command) ; // debug line
        // printf("first occ [I]: %d\n", I) ; // debug line

        if(I == -1) {
            I = total_unique_process ;
            total_unique_process++ ;
        }

        if(burst_history[I][k-1] != 0) {
            for(int j = 0 ; j < k-1 ; j++) {
                burst_history[I][j] = burst_history[I][j+1] ;
            }
            burst_history[I][k-1] = context_finish_time - context_start_time ;
        } else {
            for(int j = 0 ; j < k ; j++) {
                if(burst_history[I][j] == 0) {
                    burst_history[I][j] = context_finish_time - context_start_time ;
                    break ;
                }
            }
        }

        done_cnt++ ;

        process_info[p_idx].finished = WIFEXITED(status) ;
        process_info[p_idx].error = !WIFEXITED(status) || WEXITSTATUS(status) != 0 ;

        process_info[p_idx].done = 1 ;
        process_info[p_idx].completion_time = current_time_ms() - start_time_global ;
        process_info[p_idx].turnaround_time = process_info[p_idx].completion_time - process_info[p_idx].arival_time ;
        process_info[p_idx].burst_time += (context_finish_time - context_start_time);
        process_info[p_idx].total_burst_time += (context_finish_time - context_start_time);
        process_info[p_idx].waiting_time = process_info[p_idx].turnaround_time - process_info[p_idx].burst_time ;
        process_info[p_idx].run_count++ ;

        printf("%s, %lu, %lu\n",
            process_info[p_idx].command,
            context_start_time,
            context_finish_time);

        // Write CSV entry
        FILE *csv = fopen("result_online_SJF.csv", "a");
        fprintf(csv, "%s,%s,%s,%lu,%lu,%lu,%lu\n",
            process_info[p_idx].command,
            process_info[p_idx].finished ? "Yes" : "No",
            process_info[p_idx].error ? "Yes" : "No",
            process_info[p_idx].completion_time,
            process_info[p_idx].turnaround_time,
            process_info[p_idx].waiting_time,
            process_info[p_idx].response_time);
        fclose(csv);


    }

}



/*
Initial Priority: 
All newly arriving processes should start at the Medium priority level.

Priority Updates: 
When a process reappears, its priority should be revised according 
to its average burst time recorded so far, and it should be placed in 
the highest queue for which average burst time is less than allotted 
time slice. Only consider the process which didn’t end with Error
*/

int print_process_info(Process* p , int** queue , int* qsize) {
    for(int q = 0 ; q < 3 ; q++) {
        if(qsize[q] == 0) continue ;
        printf("Queue %d: (size: %d)", q, qsize[q]) ;
        for(int i = 0 ; i < qsize[q] ; i++) {
            int idx = queue[q][i] ;
            printf("%s (avg_burst: %.2f) | ", p[idx].command , p[idx].avg_burst) ;
        }
        printf("\n") ;
    }
}

int input_process_MLFQ(Process* p , int total_process , int** queue , int* qsize , uint64_t quantum0, uint64_t quantum1, uint64_t quantum2) {
    char *line = NULL;
    size_t linecap = 0;
    ssize_t nread;

    bool x = 0 ; // debug variable

    /* Read lines until EOF */
    while((nread = getline(&line, &linecap, stdin)) != -1) {
        
        x=1 ;

        /* Remove trailing newline if present */
        if(nread > 0 && line[nread - 1] == '\n') {
            line[nread - 1] = '\0';
            nread--;
        }

        /* Skip empty lines (optional) */
        if(nread == 0) continue;

        int idx = total_process;
        total_process++;

        p[idx].arival_time = current_time_ms() - start_time_global ;

        p[idx].command = (char *)malloc(MAX_COMMAND_LEN);
        if(!p[idx].command) {
            perror("malloc");
            /* On allocation failure mark the process entry error and continue */
            p[idx].error = 1;
            p[idx].done = 1;
            continue;
        }

        /* Copy at most MAX_COMMAND_LEN-1 chars and null-terminate */
        strncpy(p[idx].command, line, MAX_COMMAND_LEN - 1);
        p[idx].command[MAX_COMMAND_LEN - 1] = '\0';

        p[idx].finished = 0;
        p[idx].error = 0;
        p[idx].started = 0;
        p[idx].burst_time = 0;
        p[idx].total_burst_time = 0;
        p[idx].avg_burst = INITIAL_AVG_BURST;
        p[idx].run_count = 0;
        p[idx].done = 0;
        p[idx].process_id = 0;

        // iterate on all processess and check if this process is already there
        // if yes, then copy its avg_burst and run_count
        for(int i = 0; i < total_process; i++) {
            if(i == idx) continue; // skip self
            if(strcmp(p[i].command, p[idx].command) == 0) {
                p[idx].total_burst_time = p[i].burst_time ;
                // p[idx].avg_burst = p[i].avg_burst;
                // p[idx].run_count = p[i].run_count;
                break;
            }
        }

        /* fork and assign pid */
        pid_t pid = fork();

        if (pid < 0) {
            /* can't fork, mark as error */
            p[idx].error = 1;
            p[idx].done = 1;
            free(p[idx].command); /* optional cleanup */
            p[idx].command = NULL;
        } else if (pid == 0) {
            /* CHILD */
            /* create a modifiable copy of command for tokenization */
            char *cmd = strdup(p[idx].command ? p[idx].command : "");
            if (!cmd) _exit(1);

            char *saveptr;
            char *token = strtok_r(cmd, " ", &saveptr);
            /* build argv */
            char *args[MAX_COMMAND_LEN];
            int j = 0;
            while (token && j < MAX_COMMAND_LEN - 1) {
                args[j++] = token;
                token = strtok_r(NULL, " ", &saveptr);
            }
            args[j] = NULL;

            execvp(args[0], args);
            free(cmd);
            _exit(1);
        } else {
            /* PARENT */
            /* Immediately stop the child so scheduler can manage it later */
            kill(pid, SIGSTOP);
            p[idx].process_id = pid;
        }

        /* insert into the queue at start priority */
        if(p[idx].total_burst_time == 0) {

            queue[START_PRIORITY][qsize[START_PRIORITY]] = idx;
            qsize[START_PRIORITY]++;

        } else {
            
            int priority = (p[idx].total_burst_time < (double)quantum0) ? 0 : (p[idx].total_burst_time < (double)quantum1) ? 1 : 2 ;
            queue[priority][qsize[priority]] = idx;
            qsize[priority]++;

        }
    }

    free(line); /* free buffer allocated by getline */

    // if(x) {
    //     print_process_info(p , queue , qsize) ;
    // }

    return total_process;
}


void MultiLevelFeedbackQueue(int quantum0_, int quantum1_, int quantum2_, int boostTime_) {

    // times given in sec, converting them to ms
    uint64_t quantum0 = quantum0_ ;
    uint64_t quantum1 = quantum1_ ;
    uint64_t quantum2 = quantum2_ ;
    uint64_t boostTime = boostTime_ ;

    FILE *csv = fopen("result_online_MLFQ.csv", "w");
    fclose(csv);

    Process process_info[MAX_PROCESS] ;
    int **queue = (int**)malloc(3 * sizeof(int*));
    for (int i = 0; i < 3; i++) {
        queue[i] = (int*)malloc(MAX_PROCESS * sizeof(int));
    }
    int qsize[3] = {0,0,0} ;
    int total_process = 0 ;
    
    // Set stdin non-blocking so we can check for new commands
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    
    start_time_global = current_time_ms();
    uint64_t last_boost = current_time_ms();

    while(1) { for(int lvl = 0 ; lvl < 3 ; lvl++) {

        
        uint64_t quantum = (lvl == 0) ? quantum0 : (lvl == 1) ? quantum1 : quantum2 ;
        struct timespec ts;
        ts.tv_sec = quantum / 1000;
        ts.tv_nsec = (quantum % 1000) * 1000000;
        // convert to ns
        
        // this is loop is for, when there is no process in MLFQ, then below while loop
        // will not run, and we have to take input this way then
        if(qsize[lvl] == 0) {
            // check for input
            total_process = input_process_MLFQ(process_info , total_process , queue , qsize, quantum0, quantum1, quantum2) ;
        }

        // complete this level no
        while(qsize[lvl] > 0) {

            int p_idx = queue[lvl][0] ; // get the first process
            for(int i = 1 ; i < qsize[lvl] ; i++) {
                queue[lvl][i-1] = queue[lvl][i] ;
            }
            qsize[lvl]-- ;

            if(process_info[p_idx].done) continue ;

            // printf("lvl: %d , command: %s\n", lvl, process_info[p_idx].command) ; // debug line

            
            if(!process_info[p_idx].started) {
                
                // printf("start kiya\n") ; // debug line
                
                process_info[p_idx].start_time = current_time_ms() - start_time_global ;
                process_info[p_idx].response_time = current_time_ms() - start_time_global - process_info[p_idx].arival_time ;
                process_info[p_idx].started = 1 ;
            }
            
            pid_t ret ;
            int status ;
            uint64_t wait_time = 0;
            
            uint64_t context_start_time = current_time_ms() - start_time_global ;
            uint64_t temp_context_start_time = current_time_ms() ;

            // resume
            kill(process_info[p_idx].process_id , SIGCONT) ;

            // wait 
            while(current_time_ms() - temp_context_start_time < quantum) {
                ret = waitpid(process_info[p_idx].process_id , &status , WNOHANG) ;
                if(ret != 0) break;
            }


            // pause
            kill(process_info[p_idx].process_id , SIGSTOP) ;
            
            uint64_t context_end_time = current_time_ms() - start_time_global ;

            process_info[p_idx].burst_time += (context_end_time - context_start_time);
            // process_info[p_idx].total_burst_time += (context_end_time - context_start_time);
            // process_info[p_idx].run_count++ ;
            // process_info[p_idx].avg_burst = (double) ((double)process_info[p_idx].total_burst_time / (double)process_info[p_idx].run_count) ;


            if(ret == 0) {
                // still running
                // kill(process_info[p_idx].process_id , SIGSTOP) ;

                // Print context switch info
                printf("%s, %lu, %lu\n",
                    process_info[p_idx].command,
                    context_start_time,
                    context_end_time);

                // assign next level
                int next_priority = -1 ;
                if(process_info[p_idx].total_burst_time > 0) {
                    next_priority = (process_info[p_idx].total_burst_time < (double)quantum0) ? 0 : (process_info[p_idx].total_burst_time < (double)quantum1) ? 1 : 2 ;
                } else {
                    next_priority = (process_info[p_idx].burst_time < (double)quantum0) ? 0 : (process_info[p_idx].burst_time < (double)quantum1) ? 1 : 2 ;
                }

                // insert into the queue
                queue[next_priority][qsize[next_priority]] = p_idx ;
                qsize[next_priority]++ ;

            } else {

                // Print context switch info
                printf("%s, %lu, %lu\n",
                    process_info[p_idx].command,
                    context_start_time,
                    context_end_time);

                // process conpleted
                process_info[p_idx].done = 1 ;
                process_info[p_idx].finished = WIFEXITED(status) ; 
                process_info[p_idx].error = !WIFEXITED(status) || WEXITSTATUS(status) != 0 ;
                process_info[p_idx].completion_time = current_time_ms() - start_time_global ;
                process_info[p_idx].turnaround_time = process_info[p_idx].completion_time - process_info[p_idx].arival_time ;
                process_info[p_idx].waiting_time = process_info[p_idx].turnaround_time - process_info[p_idx].burst_time ;

                // Write CSV entry
                csv = fopen("result_online_MLFQ.csv", "a");
                fprintf(csv, "%s,%s,%s,%lu,%lu,%lu,%lu\n",
                    process_info[p_idx].command,
                    process_info[p_idx].finished ? "Yes" : "No",
                    process_info[p_idx].error ? "Yes" : "No",
                    process_info[p_idx].completion_time,
                    process_info[p_idx].turnaround_time,
                    process_info[p_idx].waiting_time,
                    process_info[p_idx].response_time);
                fclose(csv);
            }

            // time to do context switch
            // check for input
            total_process = input_process_MLFQ(process_info , total_process , queue , qsize, quantum0, quantum1, quantum2) ;

            // check for boost
            if(current_time_ms() - last_boost > boostTime) {
                for(int L = 1 ; L < 3 ; L++) {
                    for(int i = 0 ; i < qsize[L] ; i++) {
                        queue[0][qsize[0]] = queue[L][i] ;
                        qsize[0]++ ;
                    }
                    qsize[L] = 0 ;
                }
                last_boost = current_time_ms() ;

                // printf("Boosted\n") ; // debug line
            }

            // print_process_info(process_info , queue , qsize) ; // debug line
            
        }

    } }

}
