# devchat
A cruddy kernel-mode system-wide chat program for FreeBSD

This creates a device file `/dev/chat` which will keep a record of the last 255 messages (by default) which were written to it. It also includes an ioctl to clear the chat log.
