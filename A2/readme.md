Hi ta/professor, my team pass the given test code, however we didn't use the linked list for msg_queue_poll() and here is how it works. 

In the msg_queue_poll(), we malloc a condition variable called "cv". Then we iterate the fds, and let each queue points toward the "cv". As you can see you in the code, there is condition variable called "mallocCV" in each mq_backend, and we assign "cv" to "mallocCV".

Next, when we use a for loop to iterate through the fds, we check if there's an event occurs to the queue. When an event happens, we change the fds.revent accordingly. If after one iteration of the for loop, none of the event happens to any of the fds, we will wait on the "cv".

If there is something readable or writeable for queue, it will trigger the "cv" in the read/write function to notify the cv waiting in msg_queue_poll(). Once the "cv" has been trigger in msg_queue_poll(), it will go through the for loop once again to check any event happening.

In the end, if one or more event happens, after one iteration of the for loop, msg_queue_poll() will return the numbers of revent modify.