/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
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

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)

//hash表节点，Key-Value节点
typedef struct dictEntry {
    //hash表节点的key
    void *key;

    //hash表节点的value
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;

    //指向下个hash表节点，形成链表
    struct dictEntry *next;
} dictEntry;

//保存一簇用于操作特定键值对的函数指针
typedef struct dictType {
    //计算hash值
    uint64_t (*hashFunction)(const void *key);
    //复制键
    void *(*keyDup)(void *privdata, const void *key);
    //复制值
    void *(*valDup)(void *privdata, const void *obj);
    //比较键
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    //销毁键
    void (*keyDestructor)(void *privdata, void *key);
    //销毁值
    void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
//hash表节点，Key-Value节点
typedef struct dictht {
    //hash表数组
    dictEntry **table;
    //hash表大小
    unsigned long size;
    //hash表大小掩码，用于计算索引值，总是等于size - 1
    unsigned long sizemask;
    //hash表现有节点数量
    unsigned long used;
} dictht;

//Redis Dict 中定义了两张哈希表，是为了后续字典的扩展作Rehash之用
typedef struct dict {

    /*
    type和privdata属性是针对不同类型的键值对，为创建多态字典而设置的
    */
    //一个指向dictType结构的指针（type）。它通过自定义的方式使得dict的key和value能够存储任何类型的数据。
    dictType *type;

    //一个私有数据指针。由调用者在创建dict的时候传进来。
    void *privdata;

    //两个哈希表，只有在rehash的过程中，ht[0]和ht[1]才都有效。一般情况下，只有ht[0]有效，ht[1]里面没有任何数据。
    dictht ht[2];

    //rehash索引，记录rehash进度的标志，-1表示rehash未进行
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */

    //当前正在进行遍历的iterator的个数。
    unsigned long iterators; /* number of iterators currently running */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */

/*
迭代器是用于迭代遍历字典中所有的节点的一个工具，有两种，一种是安全迭代器，
一种是不安全迭代器。安全迭代器就是指，在迭代的过程中，允许对字典结构进行修改
，也即允许你添加、删除、修改字典中的键值对节点。
不安全迭代器即不允许对字典中任何节点进行修改。
*/
typedef struct dictIterator {
    //指向一个即将被迭代的字典结构
    dict *d;
    // 记录了当前迭代到字典中的桶索引
    long index;
    //table 取值为 0 或 1，表示当前迭代的是字典中哪个哈希表，
    //safe 标记当前迭代器是安全的或是不安全的。
    int table, safe;
    //entry记录的是当前迭代的节点，nextEntry的值等于entry的 
    //next指针，用于防止当前节点接受删除操作后续节点丢失情况。
    dictEntry *entry, *nextEntry;

    /* unsafe iterator fingerprint for misuse detection. 

    fingerprint 保存了dictFingerprint函数根据当前字典的基本
    信息计算的一个指纹信息，稍有一丁点变动，指纹信息就会发生变化，
    用于不安全迭代器检验。
    */
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);
typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
//字典释放val
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

//字典val函数复制时候调用，如果dict中的dictType定义了这个函数指针
#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

#define dictSetSignedIntegerVal(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

#define dictSetDoubleVal(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)

//调用dictType定义的key析构函数
#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)
//key复制
#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)
//调用dictType定义的key比较函数，没有定义直接key值直接比较
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

//获取指定key的哈希值
#define dictHashKey(d, key) (d)->type->hashFunction(key)
//获取指定节点的key
#define dictGetKey(he) ((he)->key)
//获取指定节点的value
#define dictGetVal(he) ((he)->v.val)
//获取指定节点的value，值为signed int
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
//获取指定节点的value，值为unsigned int
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
//获取指定节点的value，值为double
#define dictGetDoubleVal(he) ((he)->v.d)
//获取字典中哈希表的总长度，总长度=哈希表1散列数组长度+哈希表2散列数组长度
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
//获取字典中哈希表已被使用的节点数量，已被使用的节点数量 = 
//哈希表1散列数组已被使用的节点数量+哈希表2散列数组已被使用的节点数量
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
// 字典当前是否正在进行rehash操作
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/* API */
dict *dictCreate(dictType *type, void *privDataPtr);
int dictExpand(dict *d, unsigned long size);
int dictAdd(dict *d, void *key, void *val);
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing);
dictEntry *dictAddOrFind(dict *d, void *key);
int dictReplace(dict *d, void *key, void *val);
int dictDelete(dict *d, const void *key);
dictEntry *dictUnlink(dict *ht, const void *key);
void dictFreeUnlinkedEntry(dict *d, dictEntry *he);
void dictRelease(dict *d);
dictEntry * dictFind(dict *d, const void *key);
void *dictFetchValue(dict *d, const void *key);
int dictResize(dict *d);
dictIterator *dictGetIterator(dict *d);
dictIterator *dictGetSafeIterator(dict *d);
dictEntry *dictNext(dictIterator *iter);
void dictReleaseIterator(dictIterator *iter);
dictEntry *dictGetRandomKey(dict *d);
dictEntry *dictGetFairRandomKey(dict *d);
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);
void dictGetStats(char *buf, size_t bufsize, dict *d);
uint64_t dictGenHashFunction(const void *key, int len);
uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len);
void dictEmpty(dict *d, void(callback)(void*));
void dictEnableResize(void);
void dictDisableResize(void);
int dictRehash(dict *d, int n);
int dictRehashMilliseconds(dict *d, int ms);
void dictSetHashFunctionSeed(uint8_t *seed);
uint8_t *dictGetHashFunctionSeed(void);
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, dictScanBucketFunction *bucketfn, void *privdata);
uint64_t dictGetHash(dict *d, const void *key);
dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash);

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
