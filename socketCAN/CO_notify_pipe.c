/* Snipped from https://stackoverflow.com/a/2486353 */

#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>

#include "CO_notify_pipe.h"

struct CO_NotifyPipe {
    int m_receiveFd;
    int m_sendFd;
};

CO_NotifyPipe_t *CO_NotifyPipeCreate(void)
{
    int pipefd[2];
    CO_NotifyPipe_t *p;
    int ret = pipe(pipefd);

    if (ret < 0) {
        return NULL;
    }
    p = calloc(1, sizeof(CO_NotifyPipe_t));
    if (p == NULL) {
        return NULL;
    }
    p->m_receiveFd = pipefd[0];
    p->m_sendFd = pipefd[1];
    fcntl(p->m_sendFd,F_SETFL,O_NONBLOCK);
    return p;
}

void CO_NotifyPipeFree(CO_NotifyPipe_t *p)
{
    if (p == NULL) {
        return;
    }
    close(p->m_sendFd);
    close(p->m_receiveFd);
    free(p);
}


int CO_NotifyPipeGetFd(CO_NotifyPipe_t *p)
{
    if (p == NULL) {
        return -1;
    }
    return p->m_receiveFd;
}


void CO_NotifyPipeSend(CO_NotifyPipe_t *p)
{
    if (p == NULL) {
        return;
    }
    write(p->m_sendFd,"1",1);
}
