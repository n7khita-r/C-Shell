#ifndef BG_JOBS_H
#define BG_JOBS_H

#include <sys/types.h>

#define MAX_JOBS 100
#define MAX_CMD_LEN 256

// Job status types
typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} job_status_t;

// Structure to track background jobs
typedef struct {
    int job_id;
    pid_t pid;
    pid_t pgid;  // Process group ID
    char command[MAX_CMD_LEN];
    int active;
    job_status_t status;
} bg_job_t;

// Global variables for signal handling
extern pid_t foreground_pid;
extern pid_t foreground_pgid;

// Function declarations
void init_bg_jobs(void);
void check_background_processes(void);
int add_background_job(pid_t pid, const char* command);
void remove_background_job(pid_t pid);
int has_background_ampersand(const char* input);
char* remove_trailing_ampersand(const char* input);
int get_active_job_count(void);
void get_job_info(int index, pid_t *pid, char *command, int *status);
int find_job_by_number(int job_number);
int get_most_recent_job(void);
int get_job_number(int job_index);
void resume_job(int job_index);
void bring_job_to_foreground(int job_index);
void cleanup_all_jobs(void);

// Signal handling functions
void setup_signal_handling(void);
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void set_foreground_process(pid_t pid, pid_t pgid);
void clear_foreground_process(void);
int add_stopped_job(pid_t pid, const char* command);

// Additional job control functions
void display_jobs(void);
bg_job_t* find_job_by_id(int job_id);
int continue_job_bg(int job_id);
int continue_job_fg(int job_id);

#endif // BG_JOBS_H