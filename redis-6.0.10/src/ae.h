/* A simple event-driven programming library. Originally I wrote this code
 * for the Jim's event-loop (Jim is a Tcl interpreter) but later translated
 * it in form of a library for easy reuse.
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __AE_H__
#define __AE_H__

#include <time.h>

#define AE_OK 0
#define AE_ERR -1

#define AE_NONE 0       /* No events registered. */
#define AE_READABLE 1   /* Fire when descriptor is readable. */
#define AE_WRITABLE 2   /* Fire when descriptor is writable. */
#define AE_BARRIER 4   // 强制先写后读 
 /* With WRITABLE, never fire the event if the
                           READABLE event already fired in the same event
                           loop iteration. Useful when you want to persist
                           things to disk before sending replies, and want
                           to do that in a group fashion. 
                           */

#define AE_FILE_EVENTS (1<<0)
#define AE_TIME_EVENTS (1<<1)
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS)
#define AE_DONT_WAIT (1<<2)
#define AE_CALL_BEFORE_SLEEP (1<<3)
#define AE_CALL_AFTER_SLEEP (1<<4)

#define AE_NOMORE -1
#define AE_DELETED_EVENT_ID -1

/* Macros */
#define AE_NOTUSED(V) ((void) V)

struct aeEventLoop;

/* Types and data structures */
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);
typedef int aeTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData);
typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData);
typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);

/* File event structure 
文件事件处理与套接字相关的工作
*/
typedef struct aeFileEvent {
    /* AE_READABLE 当fd是可读的时候，mask为此值
     AE_WRITABLE 当fd是可写的时候，mask为此值
     AE_BARRIER 当读写均可用的时候，先写后读，适用于写需求优先级比较高的情况
    */
    int mask; /* one of AE_(READABLE|WRITABLE|BARRIER) */
    aeFileProc *rfileProc;
    aeFileProc *wfileProc;

    // 客户端传入的数据
    void *clientData;
} aeFileEvent;

/* Time event structure 
时间事件在aeEventLoop中以链表保存，aeCreateTimeEvent()会将新创建的时间事件添加在链表头：
*/
typedef struct aeTimeEvent {
    //时间事件的ID，全局唯一，每增加一个等同于aeEventLoop.timeEventNextId++
    long long id; /* time event identifier. */
    //秒，在时间截止时触发timeProc
    long when_sec; /* seconds */
    //毫秒，在时间截止时触发timeProc
    long when_ms; /* milliseconds */
    //时间事件处理函数
    aeTimeProc *timeProc;
    //时间事件的最后一次处理程序，若已设置，则删除时间事件时会被调用
    aeEventFinalizerProc *finalizerProc;
    void *clientData;
    struct aeTimeEvent *prev;  //时间事件链表的前节点
    struct aeTimeEvent *next;  //时间事件链表的后节点
    int refcount; /* refcount to prevent timer events from being
  		   * freed in recursive time event calls. */
} aeTimeEvent;

/* A fired event */
typedef struct aeFiredEvent {
    //就绪文件描述符
    int fd;
    // 文件事件的 读写 类型
    int mask;
} aeFiredEvent;

/* State of an event based program */
typedef struct aeEventLoop {
    //最大文件描述符的值
    int maxfd;   /* highest file descriptor currently registered */
    //文件描述符的最大监听数
    int setsize; /* max number of file descriptors tracked */
    //用于生成时间事件的唯一标识id  
    long long timeEventNextId;
    //用于检测系统时间是否变更（判断标准 now<lastTime）
    time_t lastTime;     /* Used to detect system clock skew */
    //注册要使用的文件事件，这里的分离表实现为直接索引，即通过fd来访问，实现事件的分离
    aeFileEvent *events; /* Registered events */
    //已触发的事件
    aeFiredEvent *fired; /* Fired events */
   //指向首个时间事件结构体, 而时间事件结构体里有next指针, 
   //指向下一个结构体, 实际上整体看上去是一个环形链表(最后一个时间事件结构体里的next指针会指向timeEventHead
    aeTimeEvent *timeEventHead;
   //停止标志，1表示停止
    int stop;
   //指向底层不同多路复用实现的数据结构, 可以是epoll, select, evport或者是kqueue
   //如果使用的是select，那么状态数据包含了不同的fd_set；如果使用的是epoll，
   //状态数据包含了epoll_create()返回的fd，还有用于接收epoll_wait()返回的存在可用事件的列表events。
    void *apidata; /* This is used for polling API specific data */
    // 事件循环器 新一轮循环前的钩子函数
    aeBeforeSleepProc *beforesleep;

    // 事件循环器 一轮循环后的钩子函数
    aeBeforeSleepProc *aftersleep;
    
    int flags;
} aeEventLoop;

/* Prototypes */
aeEventLoop *aeCreateEventLoop(int setsize);
void aeDeleteEventLoop(aeEventLoop *eventLoop);
void aeStop(aeEventLoop *eventLoop);
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData);
void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask);
int aeGetFileEvents(aeEventLoop *eventLoop, int fd);
long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds,
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc);
int aeDeleteTimeEvent(aeEventLoop *eventLoop, long long id);
int aeProcessEvents(aeEventLoop *eventLoop, int flags);
int aeWait(int fd, int mask, long long milliseconds);
void aeMain(aeEventLoop *eventLoop);
char *aeGetApiName(void);
void aeSetBeforeSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *beforesleep);
void aeSetAfterSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *aftersleep);
int aeGetSetSize(aeEventLoop *eventLoop);
int aeResizeSetSize(aeEventLoop *eventLoop, int setsize);
void aeSetDontWait(aeEventLoop *eventLoop, int noWait);

#endif
