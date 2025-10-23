/* PlanC (hubenchang0515@outlook.com) -- an example application
 * that implements a custom conversation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <security/pam_appl.h>
#include <sys/ioctl.h>

/***************************************
 * @brief echo off/on
 * @param[in] fd file descriptor
 * @param[in] off 1 - echo off，0 - echo on
 ***************************************/
static void echoOff(int fd, int off)
{
    struct termios tty;
    if (ioctl(fd, TCGETA, &tty) < 0)
    {
        fprintf(stderr, "TCGETA failed: %s\n", strerror(errno));
        return;
    }

    if (off)
    {
        tty.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
        if (ioctl(fd, TCSETAF, &tty) < 0)
        {
            fprintf(stderr, "TCSETAF failed: %s\n", strerror(errno));
        }
    }
    else
    {
        tty.c_lflag |= (ECHO | ECHOE | ECHOK | ECHONL);
        if (ioctl(fd, TCSETAW, &tty) < 0)
        {
            fprintf(stderr, "TCSETAW failed: %s\n", strerror(errno));
        }
    }
}

/***************************************
 * @brief echo off stdin
 ***************************************/
static void echoOffStdin(void)
{
    echoOff(fileno(stdin), 1);
}

/***************************************
 * @brief echo on stdin
 ***************************************/
static void echoOnStdin(void)
{
    echoOff(fileno(stdin), 0);
}

/***************************************
 * @brief read a line input
 * @return the input string
 ***************************************/
static char *readline(void)
{
    char input[PAM_MAX_RESP_SIZE];
    int i;

    flockfile(stdin);
    for (i = 0; i < PAM_MAX_RESP_SIZE - 1; i++)
    {
        int ch = getchar_unlocked();
        if (ch == '\n' || ch == '\r' ||ch == EOF)
            break;
        input[i] = ch;
    }
    funlockfile(stdin);
    input[i] = '\0';

    return (strdup(input));
}

/**************************************************
 * @brief callback of PAM conversation
 * @param[in] num_msg the count of message
 * @param[in] msg PAM message
 * @param[out] resp our response
 * @param[in] appdata_ptr custom data passed by struct pam_conv.appdata_ptr
 * @return state
 **************************************************/
static int conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
    (void)(appdata_ptr);
    int i;

    /* check the count of message */
    if (num_msg <= 0 || num_msg >= PAM_MAX_MSG_SIZE)
    {
        fprintf(stderr, "invalid num_msg(%d)\n", num_msg);
        return PAM_CONV_ERR;
    }

    /* alloc memory for response */
    if ((resp[0] = malloc(num_msg * sizeof(struct pam_response))) == NULL)
    {
        fprintf(stderr, "bad alloc\n");
        return PAM_BUF_ERR;
    }

    /* response for message */
    for (i = 0; i < num_msg; i++)
    {
        const struct pam_message *m = *msg + i;
        struct pam_response *r = *resp + i;
        r->resp_retcode = 0;    /* currently un-used, zero expected */
        switch (m->msg_style)
        {
        case PAM_PROMPT_ECHO_OFF:   /* get the input with echo off, like the password */
            printf("%s", m->msg);
            echoOffStdin();
            r->resp = readline();
            echoOnStdin();
            printf("\n");
            break;

        case PAM_PROMPT_ECHO_ON:    /* get the input with echo on, like the username */
            printf("%s", m->msg);
            r->resp = readline();
            break;

        case PAM_TEXT_INFO:         /* normal info */
            printf("%s\n", m->msg);
            break;

        case PAM_ERROR_MSG:         /* error info */
            fprintf(stderr, "%s\n", m->msg);
            break;

        default:
            fprintf(stderr, "unexpected msg_style: %d\n", m->msg_style);
            break;
        }
    }
    return PAM_SUCCESS;
}

int main(void)
{
    struct pam_conv pam_conv = {conversation, NULL};
    pam_handle_t *pamh;

    /* echo on while exist, like Ctrl+C on input password */
    atexit(echoOnStdin);

    if (PAM_SUCCESS != pam_start("login", NULL, &pam_conv, &pamh))
    {
        fprintf(stderr, "pam_start failed\n");
        return EXIT_FAILURE;
    }

    if (PAM_SUCCESS != pam_authenticate(pamh, 0))
    {
        fprintf(stderr, "pam_authenticate failed\n");
        pam_end(pamh, 0);
        return EXIT_FAILURE;
    }

    if (PAM_SUCCESS != pam_acct_mgmt(pamh, 0))
    {
        fprintf(stderr, "pam_acct_mgmt failed\n");
        pam_end(pamh, 0);
        return EXIT_FAILURE;
    }

    pam_end(pamh, 0);
    return EXIT_SUCCESS;
}
