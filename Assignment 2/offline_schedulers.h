#pragma once

//Can include any other headers as needed
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>



#include <stdint.h>


typedef struct {

    //This will be given by the tester function this is the process command to be scheduled
    char *command;

    //Temporary parameters for your usage can modify them as you wish
    bool finished;  //If the process is finished safely
    bool error;    //If an error occurs during execution
    uint64_t start_time;
    uint64_t completion_time;
    uint64_t turnaround_time;
    uint64_t waiting_time;
    uint64_t response_time;
    bool started; 
    int process_id;
    
    uint64_t burst_time; // Total time spent running on CPU
    bool done ;

} Process;

// Function prototypes
void FCFS(Process p[], int n);
void RoundRobin(Process p[], int n, int quantum_);
void MultiLevelFeedbackQueue(Process p[], int n, int quantum0_, int quantum1_, int quantum2_, int boostTime_);

/*
theory : 

start_time	    When process first runs on CPU (ms)
completion_time	When process finishes (ms)
turnaround_time	Total time in system = completion_time - start_time
waiting_time	Total time spent waiting in ready queue
response_time	Time from arrival to first CPU execution

struct timeval is a structure defined in <sys/time.h> (on Linux / Unix systems).
It represents a time value with microsecond precision.

struct timeval {
    time_t      tv_sec;   // seconds
    suseconds_t tv_usec;  // microseconds
};

struct timespec, represents a time with nanosecond precision.
is defined in the header <time.h> (or <ctime> in C++).
struct timespec {
    time_t tv_sec;   // seconds
    long   tv_nsec;  // nanoseconds (0 to 999,999,999)
};


fork:
Creates a new process.
pid_t pid = fork();
Returns:
    0 in the child,
    > 0 (child PID) in the parent,
    < 0 on error.
*/

const int MAX_COMMAND_LEN = 3000 ;

struct timeval tv ;
uint64_t current_time_ms() {
    gettimeofday(&tv , NULL) ;
    return tv.tv_sec * 1000ULL + tv.tv_usec / 1000 ;
}

// FCFS function :
void FCFS(Process p[] , int n) {

    FILE *csv = fopen("result_offline_FCFS.csv", "w");
    fclose(csv) ;

    uint64_t start_time_global = current_time_ms() ;

    // printf("Start time global = %lu\n" , start_time_global) ; // debug line

    // variable storing start time and is global for this function
    // this is stored in mili sec

    for(int i = 0 ; i < n ; i++) { // loop to iterate over all processes (first come first Served)
        
        // save the start time of the process and mark it as started
        p[i].start_time = current_time_ms() - start_time_global ;

        // printf("Process %d start time = %lu\n" , i , p[i].start_time) ; // debug line

        p[i].response_time = p[i].start_time ; // Time from arrival to first CPU execution
        p[i].started = 1 ;

        pid_t pid = fork() ;

        if(pid < 0) { 
            
            // can't fork
            // that is can't start the child process
            // mark it as error
            p[i].error = 1 ;
            p[i].finished = 0 ;
            continue ;

        } else if(pid == 0) { // CHILD
            
            char *cmd = strdup(p[i].command) ;
            char *token = strtok(cmd , " ") ; 
            /*
            Makes a copy of the i-th process’s command string on the heap.
            We copy it because strtok() modifies the string, and we don’t want to modify the original.
            
            strtok() : It replaces the delimiter with \0 to terminate the token string.
            Returns a pointer to the first token.
            Note: no new memory allocation here; the tokens point inside cmd.
            */

            char *args[MAX_COMMAND_LEN] ; int j = 0 ;
            /*
            This will store pointers to each word (token) of the command.
            Example: if command = "ls -l /home", args[0] = "ls", args[1] = "-l", args[2] = "/home".

            j keeps track of how many tokens we’ve extracted.
            */

            while(token && j < MAX_COMMAND_LEN-1) {
                
                args[j] = token ; j++ ;
                // Note: no new memory allocation here; the tokens point inside cmd.

                token = strtok(NULL , " ") ;
                /*
                Passing NULL tells strtok() “Continue from where you left off last time.”
                strtok() uses internal static state (inside the function) to remember:
                    The last string being tokenized
                    The position in that string
                It finds the next token, replaces the next delimiter with \0, and updates its internal pointer again.
                */
            }

            args[j] = NULL ;
            
            execvp(args[0] , args) ;
            /*
            args[0] is name of the program to execute
            args is NULL-terminated array of argument strings

            Replaces the current process image with a new program.
            ** If successful, the current code stops running, and the new program starts executing.
            If it fails, it returns -1 and sets errno.
            */

            // now if this line is being exicuted then unable to exicute this child program
            // there is no meaning to mark error etc here,
            // as it will modify separate copy of child variable 
            // and parent will be not affected
            
            free(cmd); // free the duplicated string
            exit(1) ; // exit this child process

        } else { // PARENT
            p[i].process_id = pid ; // child's pid
            int status ; // Declares a variable to hold exit information of the child.

            waitpid(pid , &status , 0) ;
            // Parent waits for this child to finish using waitpid()

            p[i].completion_time = current_time_ms() - start_time_global ;

            p[i].finished = WIFEXITED(status) ;
            p[i].error = !WIFEXITED(status) || WEXITSTATUS(status) != 0 ;
            /*
            WIFEXITED(status) : process terminated normally.
            WEXITSTATUS(status) : child’s return code.
            */

            p[i].turnaround_time = p[i].completion_time /*- p[i].start_time*/ ;
            p[i].waiting_time = 0 ;

            // Context switch output (in terminal)
            // in FCFS context change is same as process complition
            printf("%s, %lu, %lu\n",
                    p[i].command,
                    p[i].start_time,
                    p[i].completion_time);
            
            // Write CSV entry
            FILE *csv = fopen("result_offline_FCFS.csv", "a");
            fprintf(csv, "%s,%s,%s,%lu,%lu,%lu,%lu\n",
                p[i].command,
                p[i].finished ? "Yes" : "No",
                p[i].error ? "Yes" : "No",
                p[i].completion_time,
                p[i].turnaround_time,
                p[i].waiting_time,
                p[i].response_time);
            fclose(csv) ;
        }
    }
}

void RoundRobin(Process p[], int n, int quantum_) {

    uint64_t quantum = quantum_ ;

    FILE *csv = fopen("result_offline_RR.csv", "w");
    fclose(csv) ;

    // Create all child processes initially
    int done_cnt = 0 ;
    for (int i = 0; i < n; i++) {
        p[i].finished = 0 ;
        p[i].error = 0 ;
        p[i].started = 0 ;
        p[i].waiting_time = 0 ;
        p[i].response_time = 0 ;
        p[i].burst_time = 0 ;
        p[i].done = 0 ;

        pid_t pid = fork() ;

        if (pid < 0) {
            // cant fork
            
            p[i].error = 1;
            p[i].done = 1 ;
            done_cnt++ ;
            continue;

        } else if (pid == 0) { // CHILD
            
            char *cmd = strdup(p[i].command) ;
            char *token = strtok(cmd, " ") ;
            char *args[MAX_COMMAND_LEN] ; int j = 0 ;
            
            while (token && j < MAX_COMMAND_LEN-1) {
                args[j] = token ; j++ ;
                token = strtok(NULL, " ") ;
            }
            args[j] = NULL;

            execvp(args[0], args);

            // FAILED
            // there is no meaning to mark error etc here,
            // as it will modify separate copy of child variable 
            // and parent will be not affected

            free(cmd); // free the duplicated string
            exit(1); // execvp failed
        
        } else {
            
            kill(pid, SIGSTOP);
            // Parent stores PID and immediately stops the process
            p[i].process_id = pid;
        }
    }

    // Round Robin scheduling loop
    struct timespec ts;
    ts.tv_sec = quantum / 1000;
    ts.tv_nsec = (quantum % 1000) * 1000000ULL;
    // convert to ns

    // printf("for quantum ts.tv_sec = %ld, ts.tv_nsec = %ld\n", ts.tv_sec, ts.tv_nsec);

    uint64_t start_time_global = current_time_ms();

    while(done_cnt < n) { for (int i = 0; i < n; i++) {

        // if(done == n) break ;

        if(p[i].done) continue;
        
        // Record start time on first run
        if(!p[i].started) {
            p[i].start_time = current_time_ms() - start_time_global;
            p[i].response_time = p[i].start_time ;
            p[i].started = 1;
        }
        
        
        uint64_t context_start_time = current_time_ms() - start_time_global ;
        kill(p[i].process_id, SIGCONT);
        nanosleep(&ts, NULL);
        kill(p[i].process_id, SIGSTOP);
        uint64_t context_finish_time = current_time_ms() - start_time_global ;

        p[i].burst_time += context_finish_time - context_start_time ;

        int status;
        pid_t ret = waitpid(p[i].process_id, &status, WNOHANG);


        if(ret == 0) {
            // Still running, pause it

            // Print context switch info
            printf("%s, %lu, %lu\n",
                    p[i].command,
                    context_start_time,
                    context_finish_time);

        } else {

            // Finished within this quantum
            p[i].done = 1 ;
            done_cnt++;
            
            p[i].finished = WIFEXITED(status) ; 
            //finished is true if the process ended normally, false otherwise.

            p[i].error = !WIFEXITED(status) || WEXITSTATUS(status) != 0 ;
            // WEXITSTATUS(status): Only valid if WIFEXITED(status) is true.
            // Gives the exit code of the child

            p[i].completion_time = current_time_ms() - start_time_global ;
            p[i].turnaround_time = p[i].completion_time /*- p[i].start_time*/ ;

            p[i].waiting_time = p[i].turnaround_time - p[i].burst_time ;
            // this currectly stores the cpu_burst_time
            // waiting time for the process will be (turnaround_time - cpu_burst_time) 

            // Print context switch info
            printf("%s, %lu, %lu\n",
                p[i].command,
                context_start_time,
                context_finish_time);

            // Write CSV entry

            FILE *csv = fopen("result_offline_RR.csv", "a");
            fprintf(csv, "%s,%s,%s,%lu,%lu,%lu,%lu\n",
                p[i].command,
                p[i].finished ? "Yes" : "No",
                p[i].error ? "Yes" : "No",
                p[i].completion_time,
                p[i].turnaround_time,
                p[i].waiting_time,
                p[i].response_time);
            fclose(csv);

        }
    } }

}

void MultiLevelFeedbackQueue(Process p[], int n, int quantum0_, int quantum1_, int quantum2_, int boostTime_) {

    // times given in sec, converting them to ms
    uint64_t quantum0 = quantum0_ ;
    uint64_t quantum1 = quantum1_ ;
    uint64_t quantum2 = quantum2_ ;
    uint64_t boostTime = boostTime_ ;

    FILE *csv = fopen("result_offline_MLFQ.csv", "w");
    fclose(csv);

    uint64_t start_time_global = current_time_ms();

    // queue[level][i] = index of process
    int queue[3][n] ;
    int qsize[3] = {0,0,0} ;

    // Create all child processes initially
    int done_cnt = 0 ;
    
    for(int i = 0; i < n; i++) {
        
        queue[0][qsize[0]] = i ; 
        qsize[0]++ ; 
        // Initialize all processes in highest priority queue

        p[i].finished = 0 ;
        p[i].error = 0 ;
        p[i].started = 0 ;
        p[i].waiting_time = 0 ;
        p[i].response_time = 0 ;
        p[i].burst_time = 0 ;
        p[i].done = 0 ;

        pid_t pid = fork() ;

        if (pid < 0) {
            // cant fork
            
            p[i].error = 1;
            p[i].done = 1 ;
            done_cnt++ ;
            qsize[0]-- ; // remove from queue

            continue;

        } else if (pid == 0) { // CHILD

            char *cmd = strdup(p[i].command) ;
            char *token = strtok(cmd, " ") ;
            char *args[MAX_COMMAND_LEN] ; int j = 0 ;

            while (token && j < MAX_COMMAND_LEN-1) {
                args[j] = token ; j++ ;
                token = strtok(NULL, " ") ;
            }
            args[j] = NULL;

            execvp(args[0], args);

            free(cmd); // free the duplicated string
            exit(1); // execvp failed
        
        } else { // PARENT
            
            kill(pid, SIGSTOP);
            // immediately stops the child process
            
            p[i].process_id = pid;
            // store the pid
        }
    }


    
    uint64_t last_boost = start_time_global ;

    bool boosted = 0 ;

    while(done_cnt < n) { for(int level = 0 ; level < 3 ; level++) {
        
        uint64_t quantum = (level == 0) ? quantum0 : (level == 1) ? quantum1 : quantum2 ;
        struct timespec ts;
        ts.tv_sec = quantum / 1000;
        ts.tv_nsec = (quantum % 1000) * 1000000;
        // convert to ns

        // printf("lvl: %d, siz: %d\n", level, qsize[level]) ; // debug line

        int temp_size = qsize[level] ;
        for(int ii = 0 ; ii < temp_size ; ii++) {

            int p_idx = queue[level][0] ;
            for(int i = 0 ; i < qsize[level] - 1 ; i++) {
                queue[level][i] = queue[level][i+1] ;
            }
            qsize[level]-- ;

            if(p[p_idx].done) continue ;

            if(!p[p_idx].started) {
                p[p_idx].start_time = current_time_ms() - start_time_global ;
                p[p_idx].response_time = p[p_idx].start_time ;
                p[p_idx].started = 1 ;
            }

            int status ;
            pid_t ret ;

            uint64_t context_start_time = current_time_ms() - start_time_global ;
            uint64_t temp_context_start_time = current_time_ms() ;
            
            kill(p[p_idx].process_id , SIGCONT) ;
            // nanosleep(&ts , NULL) ;
            // wait 
            while(current_time_ms() - temp_context_start_time < quantum) {
                ret = waitpid(p[p_idx].process_id , &status , WNOHANG) ;
                if(ret != 0) break;
            }

            kill(p[p_idx].process_id , SIGSTOP) ;

            uint64_t context_finish_time = current_time_ms() - start_time_global ;
            p[p_idx].burst_time += context_finish_time - context_start_time ;

            // check status
            ret = waitpid(p[p_idx].process_id , &status , WNOHANG) ;

            if(ret == 0) {
                
                // still running, pause it

                // Print context switch info
                printf("%s, %lu, %lu\n",
                    p[p_idx].command,
                    context_start_time,
                    context_finish_time);

                // append to lower level
                int next_priority = (level < 2) ? level + 1 : 2 ;

                // insert into the queue
                queue[next_priority][qsize[next_priority]] = p_idx ;
                qsize[next_priority]++ ;

            } else {

                done_cnt++ ;
                p[p_idx].done = 1 ;

                p[p_idx].finished = WIFEXITED(status) ; 
                //finished is true if the process ended normally, false otherwise.

                p[p_idx].error = !WIFEXITED(status) || WEXITSTATUS(status) != 0 ;
                // WEXITSTATUS(status): Only valid if WIFEXITED(status) is true.
                // Gives the exit code of the child

                p[p_idx].completion_time = current_time_ms() - start_time_global ;
                p[p_idx].turnaround_time = p[p_idx].completion_time /*- p[p_idx].start_time*/ ;

                p[p_idx].waiting_time = p[p_idx].turnaround_time - p[p_idx].burst_time ;
                // this currectly stores the cpu_burst_time
                // waiting time for the process will be (turnaround_time - cpu_burst_time)

                // Print context switch info
                printf("%s, %lu, %lu\n",
                    p[p_idx].command,
                    context_start_time,
                    context_finish_time);

                // Write CSV entry
                FILE *csv = fopen("result_offline_MLFQ.csv", "a");
                fprintf(csv, "%s,%s,%s,%lu,%lu,%lu,%lu\n",
                        p[p_idx].command,
                        p[p_idx].finished ? "Yes" : "No",
                        p[p_idx].error ? "Yes" : "No",
                        p[p_idx].completion_time,
                        p[p_idx].turnaround_time,
                        p[p_idx].waiting_time,
                        p[p_idx].response_time);
                fclose(csv);
            }


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

                break ;

                // printf("Boosted\n") ; // debug line
            }

            // // check for boost
            // if(current_time_ms() - last_boost > boostTime) {
            //     qsize[0] = 0 ;
            //     qsize[1] = 0 ;
            //     qsize[2] = 0 ;
            //     for(int i = 0 ; i < n ; i++) {
            //         if(p[i].done) continue ;
            //         queue[0][qsize[0]] = i ;
            //         qsize[0]++ ;
            //     }
            //     // for(int L = 1 ; L < 3 ; L++) {
            //     //     for(int i = 0 ; i < qsize[L] ; i++) {
            //     //         queue[0][qsize[0]] = queue[L][i] ;
            //     //         qsize[0]++ ;
            //     //     }
            //     //     qsize[L] = 0 ;
            //     // }
            //     last_boost = current_time_ms() ;
            //     // boosted = 1 ;

            //     // printf("Boosted\n") ; // debug line
            //     break ;
            // }

            // for(int l = 0 ; l < 3 ; l++) {
            //     printf("Queue %d: (size: %d) ", l, qsize[l]) ;
            //     for(int i = 0 ; i < qsize[l] ; i++) {
            //         int idx = queue[l][i] ;
            //         printf("%s | ", p[idx].command) ;
            //     }
            //     printf("\n") ;
            // }


        }

        // // current level is finished, 
        // // if it is not the lowest level then empty the queue
        // if(level < 2 && !boosted) {
        //     qsize[level] = 0 ;
        // }

        // if(boosted) {
        //     boosted = 0 ;
        // }
    } }
    
}
