#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock fs_lock;
void syscall_init (void);
bool valid_pointer(const void * ptr)

#endif /* userprog/syscall.h */
