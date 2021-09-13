/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Andrew Pelegris, Karen Reid
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 Karen Reid
 */

/**
 * CSC369 Assignment 2 - Message queue implementation.
 *
 * You may not use the pthread library directly. Instead you must use the
 * functions and types available in sync.h.
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
/*to be deleted added for purpose of debug*/
#include <stdio.h>

#include "errors.h"
#include "list.h"
#include "msg_queue.h"
#include "ring_buffer.h"


// Message queue implementation backend
typedef struct mq_backend {
	// Ring buffer for storing the messages
	ring_buffer buffer;

	// Reference count
	size_t refs;

	// Number of handles open for reads
	size_t readers;
	// Number of handles open for writes
	size_t writers;

	// Set to true when all the reader handles have been closed. Starts false
	// when they haven't been opened yet.
	bool no_readers;
	// Set to true when all the writer handles have been closed. Starts false
	// when they haven't been opened yet.
	bool no_writers;

	//TODO - Add the necessary synchronization variables.
    
	// record the message size that has been successfully read
    size_t message_size;

	// mutex for each queue
    mutex_t mutex;

	// cv for each queue to signal read/write
    cond_t cond;

	// cv for each poll thread
	cond_t *mallocCV;

	// indicate the consumer thread is ready when this is true
	bool can_Enter;

} mq_backend;


static int mq_init(mq_backend *mq, size_t capacity)
{
	if (ring_buffer_init(&mq->buffer, capacity) < 0) return -1;

	mq->refs = 0;

	mq->readers = 0;
	mq->writers = 0;

	mq->no_readers = false;
	mq->no_writers = false;

	//TODO
    mutex_init(&mq->mutex);
    cond_init(&mq->cond);
	mq->can_Enter = false;
	return 0;
}

static void mq_destroy(mq_backend *mq)
{
	assert(mq->refs == 0);
	assert(mq->readers == 0);
	assert(mq->writers == 0);

	//TODO
    mutex_destroy(&mq->mutex);
	cond_destroy(&mq->cond);

	ring_buffer_destroy(&mq->buffer);
}


#define ALL_FLAGS (MSG_QUEUE_READER | MSG_QUEUE_WRITER | MSG_QUEUE_NONBLOCK)

// Message queue handle is a combination of the pointer to the queue backend and
// the handle flags. The pointer is always aligned on 8 bytes - its 3 least
// significant bits are always 0. This allows us to store the flags within the
// same word-sized value as the pointer by ORing the pointer with the flag bits.

// Get queue backend pointer from the queue handle
static mq_backend *get_backend(msg_queue_t queue)
{
	mq_backend *mq = (mq_backend*)(queue & ~ALL_FLAGS);
	assert(mq);
	return mq;
}

// Get handle flags from the queue handle
static int get_flags(msg_queue_t queue)
{
	return (int)(queue & ALL_FLAGS);
}

// Create a queue handle for given backend pointer and handle flags
static msg_queue_t make_handle(mq_backend *mq, int flags)
{
	assert(((uintptr_t)mq & ALL_FLAGS) == 0);
	assert((flags & ~ALL_FLAGS) == 0);
	return (uintptr_t)mq | flags;
}


static msg_queue_t mq_open(mq_backend *mq, int flags)
{
	++mq->refs;

	if (flags & MSG_QUEUE_READER) {
		++mq->readers;
		mq->no_readers = false;
	}
	if (flags & MSG_QUEUE_WRITER) {
		++mq->writers;
		mq->no_writers = false;
	}

	return make_handle(mq, flags);
}

// Returns true if this was the last handle
static bool mq_close(mq_backend *mq, int flags)
{
	assert(mq->refs != 0);
	assert(mq->refs >= mq->readers);
	assert(mq->refs >= mq->writers);

	if (flags & MSG_QUEUE_READER) {
		if (--mq->readers == 0) mq->no_readers = true;
	}
	if (flags & MSG_QUEUE_WRITER) {
		if (--mq->writers == 0) mq->no_writers = true;
	}

	if (--mq->refs == 0) {
		assert(mq->readers == 0);
		assert(mq->writers == 0);
		return true;
	}
	return false;
}


msg_queue_t msg_queue_create(size_t capacity, int flags)
{
	if (flags & ~ALL_FLAGS) {
		errno = EINVAL;
		report_error("msg_queue_create");
		return MSG_QUEUE_NULL;
	}

	mq_backend *mq = (mq_backend*)malloc(sizeof(mq_backend));
	if (mq == NULL) {
		report_error("malloc");
		return MSG_QUEUE_NULL;
	}
	// Result of malloc() is always aligned on 8 bytes, allowing us to use the
	// 3 least significant bits of the handle to store the 3 bits of flags
	assert(((uintptr_t)mq & ALL_FLAGS) == 0);

	if (mq_init(mq, capacity) < 0) {
		// Preserve errno value that can be changed by free()
		int e = errno;
		free(mq);
		errno = e;
		return MSG_QUEUE_NULL;
	}

	return mq_open(mq, flags);
}

msg_queue_t msg_queue_open(msg_queue_t queue, int flags)
{
	if (!queue) {
		errno = EBADF;
		report_error("msg_queue_open");
		return MSG_QUEUE_NULL;
	}

	if (flags & ~ALL_FLAGS) {
		errno = EINVAL;
		report_error("msg_queue_open");
		return MSG_QUEUE_NULL;
	}

	mq_backend *mq = get_backend(queue);

	//TODO
	mutex_lock(&mq->mutex);
	msg_queue_t new_handle = mq_open(mq, flags);
	mutex_unlock(&mq->mutex);
	return new_handle;
}

int msg_queue_close(msg_queue_t *queue)
{
	if (!*queue) {
		errno = EBADF;
		report_error("msg_queue_close");
		return -1;
	}

	mq_backend *mq = get_backend(*queue);

	//TODO
	mutex_lock(&mq->mutex);
	if (mq_close(mq, get_flags(*queue))) {
		// Closed last handle; destroy the queue
		mutex_unlock(&mq->mutex);
		mq_destroy(mq);
		free(mq);
		*queue = MSG_QUEUE_NULL;
		return 0;
	}
	mutex_unlock(&mq->mutex);
	//TODO
	*queue = MSG_QUEUE_NULL;
	return 0;
}


ssize_t msg_queue_read(msg_queue_t queue, void *buffer, size_t length)
{
	//TODO
	// when queue is MSG_QUEUE_NULL
	if(!queue){
		errno = EBADF;
		report_error("msg_queue_read");
		return -1;
	}

    mq_backend *mq = get_backend(queue);
    mutex_lock(&mq->mutex);

    /*the size of buffer is less than the message size, return negated message size*/
    if (mq->message_size > length) {
		errno = EMSGSIZE;
		report_error("msg_queue_read");
        return -1 * mq->message_size;
    } 

    //Blocking until the queue contains at least one message.
    while (ring_buffer_used(&mq->buffer) == 0) {
        if (mq->no_writers) {
			mutex_unlock(&mq->mutex);
            return 0;
        }
		// when the queue handle is non-blocking and read would block
		if(get_flags(queue) & MSG_QUEUE_NONBLOCK){
			errno = EAGAIN;
			report_error("msg_queue_read");
        	return -1;
		}
        cond_wait(&mq->cond, &mq->mutex);
    }
    
	// if there was not enough data
    if (!ring_buffer_read(&mq->buffer, buffer, mq->message_size)) {
		return -1;
    } 

    //space available to signal write
    cond_signal(&mq->cond);

	// signal the condition variable in poll
	if(mq->can_Enter){
		cond_signal(mq->mallocCV);
	}
    mutex_unlock(&mq->mutex);

	return mq->message_size;
}

int msg_queue_write(msg_queue_t queue, const void *buffer, size_t length)
{
	//TODO
	// when queue is MSG_QUEUE_NULL
	if(!queue){
		errno = EBADF;
		report_error("msg_queue_write");
		return -1;
	}

	// when zero length message
	if(length == 0){
		errno = EINVAL;
		report_error("msg_queue_write");
		return -1;
	}

    mq_backend *mq = get_backend(queue);

    mutex_lock(&mq->mutex);

    mq->message_size = length;
	// if buffer don't have enough size
    if (mq->buffer.size < length) {
		errno = EMSGSIZE;
		report_error("msg_queue_write");
        return -1;
    }
	// if all reader handles has been close
	if(mq->no_readers){
		errno = EPIPE;
		report_error("msg_queue_write");
        return -1;
	}

    while (ring_buffer_free(&mq->buffer) < length) {
		// when the queue handle is non-blocking and write would block
		if(get_flags(queue) & MSG_QUEUE_NONBLOCK){
			errno = EAGAIN;
			report_error("msg_queue_write");
        	return -1;
		}
        cond_wait(&mq->cond, &mq->mutex);
    }
    
    ring_buffer_write(&mq->buffer, buffer, length);

	// signal cv in read
    cond_signal(&mq->cond);
	if(mq->can_Enter){
		// signal the condition variable in poll
		cond_signal(mq->mallocCV);
	}
    mutex_unlock(&mq->mutex);

    return 0;

}


int msg_queue_poll(msg_queue_pollfd *fds, size_t nfds)
{
    //TODO
	// when nfds is 0, return error
	if(nfds == 0){
		errno = EINVAL;
		report_error("msg_queue_poll");
		return -1;
	}

	// malloc a condition variable associate with each poll function
	cond_t *cv = (cond_t *)malloc(sizeof(cond_t));
	if (cv == NULL) {
		report_error("malloc");
		return -1;
	}
	cond_init(cv);
	
	msg_queue_pollfd *cur_fd;
	int all_flag = MQPOLL_WRITABLE | MQPOLL_READABLE | MQPOLL_NOREADERS | MQPOLL_NOWRITERS;
	int change = 0;
	bool found = false;

	while(!found){
		// iterate through all the fds
		for(size_t i = 0; i < nfds; i++){
			cur_fd = &fds[i];
			cur_fd->revents = 0;
			if(cur_fd->queue != MSG_QUEUE_NULL){

				// when events flag is invalid, return error
				if(cur_fd->events & ~all_flag) {
					errno = EINVAL;
					report_error("msg_queue_poll");
					return -1;
				}
				mq_backend *mq = get_backend(fds[i].queue);
				mutex_lock(&mq->mutex);
				mq->mallocCV = cv;
				mq->can_Enter = true;

				// when the event request is MQPOLL_READABLE for a non-reader queue handle, return error
				if((cur_fd->revents & MQPOLL_READABLE) && mq->no_readers){
					errno = EINVAL;
					report_error("msg_queue_poll");
					return -1;
				}

				// when the event request is MQPOLL_WRITABLE for a non-writer queue handle, return error
				if((cur_fd->revents & MQPOLL_WRITABLE) && mq->no_writers){
					errno = EINVAL;
					report_error("msg_queue_poll");
					return -1;
				}
				// when the queue is readerable
				if(ring_buffer_used(&mq->buffer) != 0 || mq->no_writers){
					cur_fd->revents |= MQPOLL_READABLE;
				}
				// when the queue is writerable
				if(ring_buffer_free(&mq->buffer) || mq->no_readers){
					cur_fd->revents |= MQPOLL_WRITABLE;
				}
				// no reader
				if(mq->no_readers){
					cur_fd->revents |= MQPOLL_NOREADERS;
				}
				// no writer
				if(mq->no_writers){
					cur_fd->revents |= MQPOLL_NOWRITERS;
				}

				if((cur_fd->events & cur_fd->revents) || !cur_fd->events){
					change++;
					found = true;
				}

			// when it is the last fd and still haven't find an event, poll function will wait
			if(i == nfds - 1){
				if(!found){
					cond_wait(cv, &mq->mutex);
				}
			}
			mutex_unlock(&mq->mutex);
			}
		}
	}

	for (size_t i = 0; i < nfds; i++){
		if(fds[i].queue != MSG_QUEUE_NULL){
			mq_backend *mq = get_backend(fds[i].queue);
			mutex_lock(&mq->mutex);
			mq->can_Enter = false;
			mutex_unlock(&mq->mutex);
		}
	}
	cond_destroy(cv);
	free(cv);
	return change;
}
