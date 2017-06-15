/*
 * Simple Echo pseudo-device KLD
 *
 * Murray Stokely
 * Søren (Xride) Straarup
 * Eitan Adler
 */

#include <sys/types.h>
#include <sys/module.h>
#include <sys/systm.h>  /* uprintf */
#include <sys/param.h>  /* defines used in kernel.h */
#include <sys/kernel.h> /* types used in module initialization */
#include <sys/conf.h>   /* cdevsw struct */
#include <sys/uio.h>    /* uio struct */
#include <sys/malloc.h>
#include <sys/errno.h>
#include "devchat.h"

#define BUFFERSIZE 255
#define LOGLEN 255
/* Function prototypes */
static d_open_t      chat_open;
static d_close_t     chat_close;
static d_read_t      chat_read;
static d_write_t     chat_write;
static d_ioctl_t     chat_ioctl;
/* Character device entry points */
static struct cdevsw chat_cdevsw = {
     .d_version = D_VERSION,
     .d_open = chat_open,
     .d_close = chat_close,
     .d_read = chat_read,
     .d_write = chat_write,
     .d_ioctl = chat_ioctl,
     .d_name = "chat",
};

struct s_chat {
     char msg[BUFFERSIZE + 1];
     struct s_chat *prev_msg;
     struct s_chat *next_msg;
     int len;
};

/* vars */
static struct cdev *chat_dev;
struct s_chat *chatmsgs_head = NULL;
struct s_chat *chatmsgs_tail = NULL;
int chatmsgs_len  = 0;

MALLOC_DECLARE(M_CHATBUF);
MALLOC_DEFINE(M_CHATBUF, "chatbuffer", "buffer for chat module");

/*
 * This function is called by the kld[un]load(2) system calls to
 * determine what actions to take when a module is loaded or unloaded.
 */
static int
chat_loader(struct module *m __unused, int what, void *arg __unused)
{
     int error = 0;

     switch (what) {
     case MOD_LOAD:                /* kldload */
	  error = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK,
			     &chat_dev,
			     &chat_cdevsw,
			     0,
			     UID_ROOT,
			     GID_WHEEL,
			     0666,
			     "chat");
	  if (error != 0)
	       break;

	  printf("Chat device loaded.\n");
	  break;
     case MOD_UNLOAD:
	  destroy_dev(chat_dev);
	  struct s_chat *it = chatmsgs_head;
	  while (it != NULL) {
	       struct s_chat *tmp = it->prev_msg;
	       free(it, M_CHATBUF);
	       it = tmp;
	  }
	  printf("Chat device unloaded.\n");
	  break;
     default:
	  error = EOPNOTSUPP;
	  break;
     }
     return (error);
}

static int
chat_open(struct cdev *dev __unused, int oflags __unused, int devtype __unused,
	  struct thread *td __unused)
{
     int error = 0;

     uprintf("Opened device \"chat\" successfully.\n");
     return (error);
}

static int
chat_close(struct cdev *dev __unused, int fflag __unused, int devtype __unused,
	   struct thread *td __unused)
{

     uprintf("Closing device \"chat\".\n");
     return (0);
}

/*
 * The read function just takes the buf that was saved via
 * chat_write() and returns it to userland for accessing.
 * uio(9)
 */
static int
chat_read(struct cdev *dev __unused, struct uio *uio, int ioflag __unused)
{
     size_t amt;
     int error;
     char msgs[LOGLEN * BUFFERSIZE + 1];
     /*
      * Read in messages formatted for the uiomove
      */
     int iter = 0;
     int len = 0;
     struct s_chat *i = chatmsgs_tail;
     if (i == NULL) {
	  return ENOMSG;
     }
     while (i != NULL) {
	  int j;
	  for(j = 0; j < BUFFERSIZE; ++j) {
	       if (i->msg[j] == (char)0)
		    break;
	       msgs[len++] = (i->msg)[j];
	  }
	  ++iter;
	  i = i->next_msg;
     }
     msgs[len] = 0;
     /*
      * How big is this read operation?  Either as big as the user wants,
      * or as big as the remaining data.  Note that the 'len' does not
      * include the trailing null character.
      */
     /* uprintf("Giving %d bytes: \n%s\n", len, msgs); */
     amt = MIN(uio->uio_resid, uio->uio_offset >= len + 1 ? 0 :
	       len + 1 - uio->uio_offset);

     if ((error = uiomove(msgs, amt, uio)) != 0)
	  uprintf("uiomove failed!\n");

     return (error);
}

/*
 * chat_write takes in a character string and saves it
 * to buf for later accessing.
 */
static int
chat_write(struct cdev *dev __unused, struct uio *uio, int ioflag __unused)
{
     size_t amt;
     int error;

     /*
      * We either write from the beginning or are appending -- do
      * not allow random access.
      *
      * Actually, we only allow write from the beginning, which acts like appending.
      */
     if (uio->uio_offset != 0)
	  return (EINVAL);

     struct s_chat *msg = malloc(sizeof(struct s_chat), M_CHATBUF, M_WAITOK);
     /* Copy the string in from user memory to kernel memory */
     amt = MIN(uio->uio_resid, (BUFFERSIZE - msg->len));

     error = uiomove(msg->msg + uio->uio_offset, amt, uio);

     /* Now we need to null terminate and record the length */
     msg->len = uio->uio_offset;
     msg->msg[msg->len] = 0;
     /* List maintenance */
     if (chatmsgs_len >= LOGLEN) {
	  chatmsgs_tail->next_msg->prev_msg = 0;
	  struct s_chat *nextptr = chatmsgs_tail->next_msg;
	  free(chatmsgs_tail, M_CHATBUF);
	  chatmsgs_tail = nextptr;
     }
     if (chatmsgs_head != NULL)
	  chatmsgs_head->next_msg = msg;
     else
	  chatmsgs_tail = msg;
     msg->prev_msg = chatmsgs_head;
     chatmsgs_head = msg;
     if (error != 0)
//	  uprintf("Write failed: bad address!\n");
     //uprintf("Successfully wrote message %s to queue with address %p, next msg is %p, prev is %p\n", msg->msg, msg, msg->next_msg, msg->prev_msg);
     chatmsgs_len++;
     return (error);
}

static int chat_ioctl(struct cdev *dev __unused, unsigned long cmd, caddr_t cmdarg __unused, int flags __unused, struct thread *td __unused) {
     if (cmd == DEVCHATCLR) {
	  struct s_chat *it = chatmsgs_head;
	  while (it != NULL) {
	       struct s_chat *tmp = it->prev_msg;
	       free(it, M_CHATBUF);
	       it = tmp;
	  }
	  chatmsgs_head = NULL;
	  chatmsgs_tail = NULL;
	  return 0;
     }
     return EDOOFUS;
}

DEV_MODULE(chat, chat_loader, NULL);
