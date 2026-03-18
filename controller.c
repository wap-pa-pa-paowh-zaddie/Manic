#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <limits.h> //has max and min values of datatypes
#include <sys/types.h> //has definitions of functions like ssize_t and pid_t
#include <sys/wait.h> // used when your program (controller.c) needs to wait for a child process (probe.c)

#include "common.h"

static int write_text_file(const char *path, const char *text)
{
    int fd = open(path, O_WRONLY);//opening the linux control file in write only mode
    if (fd < 0) 
    {
        perror("Couldn't open file! </3");
        return -1;
    }

    size_t len = strlen(text);
    ssize_t written = write(fd, text, len);// writes into the file
    if (written != (ssize_t)len) 
    {
        perror("Couldn't write into file! :(");
        close(fd);
        return -1;
    }

    if (close(fd) < 0) 
    { // closes the file
        perror("Couldn't close the file! ");
        return -1;
    }

    return 0;
}

static int write_int_file(const char *path, int value) 
{
    char buffer[32];
    int n = snprintf(buffer, sizeof(buffer), "%d", value); //returns number of characters given to buffer variable, to n
    if (n < 0 || n >= (int)sizeof(buffer)) 
    {
        perror("Couldn't convert data into text!\n");//printing into a separate stream that isn't std output, otherwise it would corrupt communication through the pipe
        return -1;
    }

    return write_text_file(path, buffer);
}

static int pwm_export(int channel) //sends pwm file 
{
    return write_int_file("/sys/class/pwm/pwmchip0/export", channel);
}

static int pwm_set_period(int period_ns) //writes pwm period of pwm to pwm control files
{
    return write_int_file("/sys/class/pwm/pwmchip0/pwm0/period", period_ns);
}

static int pwm_set_duty_cycle(int duty_ns) //writes duty cycle of pwm to pwm control files
{
    return write_int_file("/sys/class/pwm/pwmchip0/pwm0/duty_cycle", duty_ns);
}

static int pwm_enable(int enable) //turns pwm on if enable=1, turns pwm off if enable=0
{
    return write_int_file("/sys/class/pwm/pwmchip0/pwm0/enable", enable);
}

static int abs_int16(int16_t value) //for absolute value of gyro reading
{
    return (value < 0) ? -value : value;
}

static int map_sensor_to_duty_cycle(const sensor_data_t *data) 
{
    int magnitude = abs_int16((*data).gyro_z);

    if (magnitude < 500) 
    {
        return DEFAULT_PWM_PERIOD_NS / 10;   /* 10% */
    } 
    else if (magnitude < 1500) 
    {
        return DEFAULT_PWM_PERIOD_NS / 4;    /* 25% */
    } 
    else if (magnitude < 3000) 
    {
        return DEFAULT_PWM_PERIOD_NS / 2;    /* 50% */
    } 
    else 
    {
        return (DEFAULT_PWM_PERIOD_NS * 3) / 4; /* 75% */
    }
}

int main()
{
    int pipefd[2]; //stores file descriptors, pipefd[0] has read end of the pipe, pipefd[1] has write end of the pipe
    pid_t pid;

    if (pipe(pipefd) < 0) {
        perror("Pipe failed!!");
        return EXIT_FAILURE;
    }

    pid = fork();
    if (pid < 0) {
        perror("Fork failed!!");
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        /* Child process: will become probe */

        if (close(pipefd[0]) < 0) {
            perror("Failed to close read end of the pipe!");
            _exit(EXIT_FAILURE);
        }

        if (dup2(pipefd[1], STDOUT_FILENO) < 0) { //sends the stdout into the pipe
            perror("failed to send output into pipe!");
            _exit(EXIT_FAILURE);
        }

        if (close(pipefd[1]) < 0) {
            perror("Failed to close write end of the pipe!");
            _exit(EXIT_FAILURE);
        }

        execl("./probe", "./probe", (char *)NULL); // converts the child process into an independent process
        perror("controller child: execl failed");
        _exit(EXIT_FAILURE);
    }

    /* Parent process: actual controller */

    if (close(pipefd[1]) < 0) {
        perror("Failed to close write end of the pipe!");
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return EXIT_FAILURE;
    }

    if (pwm_export(0) < 0) {
        fprintf(stderr, "Failed to export pwm!\n");
    }

    usleep(100000);

    if (pwm_set_period(DEFAULT_PWM_PERIOD_NS) < 0) {
        fprintf(stderr, "Failed to set pwm period!\n");
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return EXIT_FAILURE;
    }

    if (pwm_set_duty_cycle(DEFAULT_PWM_PERIOD_NS / 10) < 0) {
        fprintf(stderr, "failed to set initial duty cycle!\n");
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return EXIT_FAILURE;
    }

    if (pwm_enable(1) < 0) {
        fprintf(stderr, "failed to enable PWM!\n");
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return EXIT_FAILURE;
    }

    while (1) {
        sensor_data_t data;
        ssize_t rd;
        int duty;

        rd = read(pipefd[0], &data, sizeof(data));

        if (rd == 0) {
            fprintf(stderr, "controller: probe closed pipe\n");
            break;
        }

        if (rd < 0) {
            perror("controller: read failed");
            break;
        }

        if (rd != (ssize_t)sizeof(data)) {
            fprintf(stderr, "controller: partial sensor packet received\n");
            break;
        }

        duty = map_sensor_to_duty_cycle(&data);

        if (pwm_set_duty_cycle(duty) < 0) {
            fprintf(stderr, "controller: failed to update duty cycle\n");
            break;
        }
    }

    if (pwm_enable(0) < 0) {
        fprintf(stderr, "controller: failed to disable PWM\n");
    }

    if (close(pipefd[0]) < 0) {
        perror("controller: close read end failed");
    }

    if (waitpid(pid, NULL, 0) < 0) {
        perror("controller: waitpid failed");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}