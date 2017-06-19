/*
 * (c) 2016 by University of Delaware, Argonne National Laboratory, San Diego 
 *     Supercomputer Center, National University of Defense Technology, 
 *     National Supercomputer Center in Guangzhou, and Sun Yat-sen University.
 *
 *     See COPYRIGHT in top-level directory.
 */
#ifndef MIMIR_COMBINE_KV_CONTAINER_H
#define MIMIR_COMBINE_KV_CONTAINER_H

#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include "kvcontainer.h"
#include "hashbucket.h"

namespace MIMIR_NS {

template <typename KeyType, typename ValType>
class CombineKVContainer : public KVContainer<KeyType, ValType>,
      public Combinable<KeyType, ValType> {
public:
    CombineKVContainer(void (*user_combine)(Combinable<KeyType,ValType> *output,
                                            KeyType *key,
                                            ValType *val1, ValType *val2,
                                            void *ptr),
                       void *user_ptr,
                       uint32_t bincount = 0,
                       int keycount = 1, int valcount = 1)
        : KVContainer<KeyType,ValType>(bincount, keycount, valcount) {

        this->user_combine = user_combine;
        this->user_ptr = user_ptr;
        bucket = NULL;
    }

    ~CombineKVContainer()
    {
    }

    int open() {

        bucket = new HashBucket<CombinerVal>();

        KVContainer<KeyType,ValType>::open();

        keyarray = (char*) mem_aligned_malloc(MEMPAGE_SIZE, MAX_RECORD_SIZE);

        LOG_PRINT(DBG_GEN, "CombineKVContainer open!\n");

        return 0;
    }

    void close() {

        this->garbage_collection();
        bucket->clear();

        mem_aligned_free(keyarray);

        delete bucket;

        KVContainer<KeyType,ValType>::close();

        LOG_PRINT(DBG_GEN, "CombineKVContainer close.\n");
    }

    int write(KeyType *key, ValType *val)
    {
        int ret = 0;

        int kvsize = this->ser->get_kv_bytes(key, val);
        if (kvsize > this->pagesize)
            LOG_ERROR("Error: KV size (%d) is larger \
                      than one page (%ld)\n", kvsize, this->pagesize);

        keybytes = this->ser->key_to_bytes(key, keyarray, MAX_RECORD_SIZE);
        u = bucket->findEntry(keyarray, keybytes);

        if (u == NULL) {
            CombinerVal tmp;

            std::unordered_map < char *, int >::iterator iter;
            for (iter = this->slices.begin(); iter != this->slices.end(); iter++) {
                char *sbuf = iter->first;
                int ssize = iter->second;

                if (ssize >= kvsize) {
                    tmp.kv = sbuf + (ssize - kvsize);
                    this->ser->kv_to_bytes(key, val, tmp.kv, kvsize);

                    if (iter->second == kvsize)
                        this->slices.erase(iter);
                    else
                        this->slices[iter->first] -= kvsize;

                    break;
                }
            }

            if (iter == this->slices.end()) {

                if (this->pageid >= this->pages.size() 
                    || this->pagesize - this->pages[this->pageid].datasize < kvsize) {
                    Page page;
                    page.datasize = 0;
                    page.buffer = (char*)mem_aligned_malloc(MEMPAGE_SIZE, this->pagesize);
                    this->pages.push_back(page);
                    this->pageid = this->pages.size() - 1;
                }

                tmp.kv = this->pages[this->pageid].buffer + this->pages[this->pageid].datasize;
                this->ser->kv_to_bytes(key, val, tmp.kv, kvsize);
                this->pages[this->pageid].datasize += kvsize;
            }
            bucket->insertEntry(tmp.kv, keybytes, &tmp);
            this->kvcount += 1;
            ret = 1;
        }
        else {
            typename SafeType<KeyType>::ptrtype u_key = NULL;
            typename SafeType<ValType>::ptrtype u_val = NULL;
            this->ser->kv_from_bytes(&u_key, &u_val, u->kv, MAX_RECORD_SIZE);
            user_combine(this, u_key, u_val, val, user_ptr);
            ret = 0;
        }
        return ret;
    }

    void update(KeyType *key, ValType *val)
    {
        typename SafeType<KeyType>::ptrtype u_key = NULL;
        typename SafeType<ValType>::ptrtype u_val = NULL;
        int ukvsize = this->ser->kv_from_bytes(&u_key, &u_val, u->kv, MAX_RECORD_SIZE);

        int kvsize = this->ser->get_kv_bytes(key, val);

        if (this->ser->compare_key(key, u_key) != 0)
            LOG_ERROR("Error: the result key of combiner is different!\n");

        if (kvsize <= ukvsize) {
            this->ser->kv_to_bytes(key, val, u->kv, kvsize);
            if (kvsize < ukvsize) {
                this->slices.insert(std::make_pair(u->kv + ukvsize - kvsize, ukvsize - kvsize));
            }
        }
        else {
            this->slices.insert(std::make_pair(u->kv, ukvsize));
            if (kvsize + this->pages[this->pageid].datasize > this->pagesize)
                this->pageid = this->add_page();
            u->kv = this->pages[this->pageid].buffer + this->pages[this->pageid].datasize;
            this->ser->kv_to_bytes(key, val, u->kv, kvsize);
            this->pages[this->pageid].datasize += kvsize;
        }

        return;
    }

    virtual int remove(KeyType *key, ValType *val, std::set<uint32_t>& remainders) {

        int ret = KVContainer<KeyType, ValType>::remove(key, val, remainders);
        if (ret != -1) {
            keybytes = this->ser->key_to_bytes(key, keyarray, MAX_RECORD_SIZE);
            bucket->removeEntry(keyarray, keybytes);
        }

        return ret;
    }

private:
    void (*user_combine)(Combinable<KeyType,ValType> *output,
                         KeyType *key, ValType *val1, ValType *val2, void *ptr);
    void *user_ptr;
    HashBucket<CombinerVal> *bucket;
    CombinerVal *u;
    char* keyarray;
    int keybytes;
};

}

#endif
