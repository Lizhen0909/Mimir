/*
 * (c) 2016 by University of Delaware, Argonne National Laboratory, San Diego 
 *     Supercomputer Center, National University of Defense Technology, 
 *     National Supercomputer Center in Guangzhou, and Sun Yat-sen University.
 *
 *     See COPYRIGHT in top-level directory.
 */
#ifndef MIMIR_INTERFACE_H
#define MIMIR_INTERFACE_H

#include <string>
#include <set>
#include "log.h"

namespace MIMIR_NS {

enum DB_POS {DB_START, DB_END};

template <typename KeyType, typename ValType>
class Base {
  public:
    virtual ~Base() {}
    virtual int open() = 0;
    virtual void close() = 0;
    virtual int seek(DB_POS pos) = 0;
    virtual uint64_t get_record_count() = 0;
};

template <typename KeyType, typename ValType>
class Readable : public Base<KeyType, ValType> {
  public:
    virtual ~Readable() {}
    virtual int read(KeyType*, ValType*) = 0;
};

template <typename KeyType, typename ValType>
class Removable : public Base<KeyType, ValType> {
  public:
    virtual ~Removable() {}
    // return 1 if success, otherwise return 0
    virtual int remove() = 0;
};

template <typename KeyType, typename ValType>
class Writable : public Base<KeyType, ValType> {
  public:
    virtual ~Writable() {}
    virtual int write(KeyType*, ValType*) = 0;
};

template <typename KeyType, typename ValType>
class Combinable {
  public:
    virtual ~Combinable() {}
};

template <typename KeyType, typename ValType>
class BaseDatabase : 
    virtual public Readable<KeyType, ValType>,
    virtual public Writable<KeyType, ValType>,
    virtual public Removable<KeyType, ValType>
{
  public:
    BaseDatabase(bool if_inner_object = false) {
        ref = 0;
        this->if_inner_object = if_inner_object;
    }

    virtual ~BaseDatabase() {
    }

    virtual int open() = 0;
    virtual void close() = 0;
    virtual int read(KeyType *, ValType *) = 0;
    virtual int seek(DB_POS pos) = 0;
    virtual int write(KeyType *, ValType *) = 0;
    virtual int remove() = 0;

    uint64_t getRef() {
        return ref;
    }

    void addRef() {
        ref ++;
    }

    void subRef() {
        ref --;
    }

    bool is_inner_object() {
        return if_inner_object;
    }

    static void addRef(BaseDatabase *d) {
        if (d != NULL) {
            d->addRef();
        }
    }

    static void subRef(BaseDatabase *d) {
	 if (d != NULL) {
            d->subRef();
            if (d->getRef() == 0 
                && d->is_inner_object() ) {
                delete d;
            }
        }
    }

  private:
    uint64_t    ref;
    bool        if_inner_object;

  public:
    static uint64_t    mem_bytes;
};

template <typename KeyType, typename ValType>
uint64_t BaseDatabase<KeyType, ValType>::mem_bytes = 0;

}

#endif

