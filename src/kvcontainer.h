//
// (c) 2016 by University of Delaware, Argonne National Laboratory, San Diego
//     Supercomputer Center, National University of Defense Technology,
//     National Supercomputer Center in Guangzhou, and Sun Yat-sen University.
//
//     See COPYRIGHT in top-level directory.
//

#ifndef MIMIR_KV_CONTAINER_H
#define MIMIR_KV_CONTAINER_H

#include <stdio.h>
#include <stdlib.h>
#include <set>
#include "container.h"
#include "containeriter.h"
#include "interface.h"
#include "serializer.h"
#include "stat.h"

namespace MIMIR_NS {

template <typename KeyType, typename ValType>
class KVContainer : virtual public BaseDatabase<KeyType, ValType>
{
  public:
    KVContainer(int keycount, int valcount)
        : BaseObject(true), BaseDatabase<KeyType, ValType>()
    {
        this->keycount = keycount;
        this->valcount = valcount;
        //this->bincount = bincount;

        pageid = 0;
        pageoff = 0;
        ptr = NULL;
        kvsize = 0;

        kvcount = 0;
        kvmem = 0;
        gbmem = 0;
        pagesize = DATA_PAGE_SIZE;

        min_kvsize = MAX_RECORD_SIZE;
        max_kvsize = 0;

        ser = new Serializer<KeyType, ValType>(keycount, valcount);

        if (std::is_pointer<KeyType>::value
            || std::is_pointer<ValType>::value) {
            ispointer = true;
        }
        else {
            ispointer = false;
        }

        LOG_PRINT(DBG_DATA, "KVContainer create.\n");
    }

    virtual ~KVContainer()
    {
        delete ser;

        for (size_t i = 0; i < pages.size(); i++) {
            mem_aligned_free(pages[i].buffer);
            BaseDatabase<KeyType, ValType>::mem_bytes -= pagesize;
        }

        LOG_PRINT(DBG_DATA, "KVContainer destory.\n");
    }

    virtual int open()
    {
        pageid = 0;
        pageoff = 0;
        ptr = NULL;
        kvsize = 0;
        LOG_PRINT(DBG_DATA, "KVContainer open.\n");
        return true;
    }

    virtual void close()
    {
        garbage_collection();
        LOG_PRINT(DBG_DATA, "KVContainer close.\n");
        return;
    }

    virtual int seek(DB_POS pos)
    {
        if (pos == DB_START) {
            pageid = 0;
            pageoff = 0;
            ptr = NULL;
            kvsize = 0;
        }
        else if (pos == DB_END) {
            if (pages.size() != 0) {
                pageid = pages.size() - 1;
                pageoff = pages[pageid].datasize;
            }
            else {
                pageid = 0;
                pageoff = 0;
            }
            ptr = NULL;
            kvsize = 0;
        }

        return true;
    }

    virtual int read(KeyType *key, ValType *val)
    {
        while (pageid < pages.size()
               && (int) pageoff >= (int) pages[pageid].datasize) {
            pageid++;
            pageoff = 0;
        }

        if (pageid >= pages.size()) {
            LOG_PRINT(DBG_DATA, "slice length=%ld\n", slices.size());
            return false;
        }
        ptr = pages[pageid].buffer + pageoff;
        kvsize = this->ser->kv_from_bytes(
            key, val, ptr, (int) (pages[pageid].datasize - pageoff));
        pageoff += kvsize;

        return true;
    }

    virtual int write(KeyType *key, ValType *val)
    {
        if (pageid >= pages.size()) {
            pageid = add_page();
        }

        ptr = pages[pageid].buffer + pages[pageid].datasize;
        kvsize = this->ser->kv_to_bytes(
            key, val, ptr, (int) (pagesize - pages[pageid].datasize));
        if (kvsize == -1) {
            pageid = add_page();
            ptr = pages[pageid].buffer + pages[pageid].datasize;
            kvsize = this->ser->kv_to_bytes(
                key, val, ptr, (int) (pagesize - pages[pageid].datasize));
            if (kvsize == -1)
                LOG_ERROR("Error: KV size (%d) is larger than one page (%ld)\n",
                          kvsize, pagesize);
        }
        pages[pageid].datasize += kvsize;

        kvcount += 1;
        kvmem += kvsize;

        if (kvsize > max_kvsize) max_kvsize = kvsize;
        if (kvsize < min_kvsize) min_kvsize = kvsize;

        return true;
    }

    virtual int remove()
    {
        if (ptr == NULL) return false;

        slices[ptr] = kvsize;

        kvcount -= 1;
        kvmem -= kvsize;
        gbmem += kvsize;

        return true;
    }

    virtual uint64_t get_record_count() { return kvcount; }

    void print(int rank, int size)
    {
        size_t count = 0;
        for (unsigned i = 0; i < slices.bucket_count(); ++i) {
            size_t bucket_size = slices.bucket_size(i);
            if (bucket_size == 0) {
                count++;
            }
            else {
                count += bucket_size;
            }
        }
        fprintf(stdout,
                "%d[%d] KVinfo: page count=%ld, kvcount=%ld, kvmem=%ld, "
                "gbmem=%ld, memuse=%ld, membytes=%ld,slices=%ld,%ld,%ld\n",
                rank, size, pages.size(), kvcount, kvmem, gbmem,
                get_mem_usage(), BaseDatabase<KeyType, ValType>::mem_bytes,
                slices.size(), slices.bucket_count(), count);
    }

  protected:
    uint64_t add_page()
    {
        Page page;
        page.datasize = 0;
        page.buffer = (char *) mem_aligned_malloc(MEMPAGE_SIZE, pagesize);
        pages.push_back(page);
        BaseDatabase<KeyType, ValType>::mem_bytes += pagesize;
        PROFILER_RECORD_COUNT(COUNTER_MAX_KV_PAGES, this->mem_bytes, OPMAX);

        return pages.size() - 1;
    }

    virtual void garbage_collection()
    {
        typename SafeType<KeyType>::ptrtype key = NULL;
        typename SafeType<ValType>::ptrtype val = NULL;
        size_t dst_pid = 0, src_pid = 0;
        Page *dst_page = NULL, *src_page = NULL;
        int64_t dst_off = 0, src_off = 0;

        if (!(this->slices.empty())) {
            LOG_PRINT(DBG_GEN, "KVContainer garbage collection: slices=%ld\n",
                      this->slices.size());

            if (kvcount == 0) {
                for (auto iter : pages) {
                    mem_aligned_free(iter.buffer);
                    BaseDatabase<KeyType, ValType>::mem_bytes -= pagesize;
                }
                pages.clear();
                //this->slices.clear();
                std::unordered_map<char *, int> empty;
                this->slices.swap(empty);
                gbmem = 0;
                return;
            }

            if (dst_pid < pages.size()) dst_page = &pages[dst_pid++];
            while (src_pid < pages.size()) {
                src_page = &pages[src_pid++];
                src_off = 0;
                while (src_off < src_page->datasize) {
                    char *src_buf = src_page->buffer + src_off;
                    std::unordered_map<char *, int>::iterator slice
                        = this->slices.find(src_buf);
                    if (slice != this->slices.end()) {
                        src_off += slice->second;
                    }
                    else {
                        int kvsize = this->ser->kv_from_bytes(
                            &key, &val, src_buf,
                            (int) (src_page->datasize - src_off));
                        if (dst_page != src_page || dst_off != src_off) {
                            if (dst_off + kvsize > this->pagesize) {
                                dst_page->datasize = dst_off;
                                dst_page = &pages[dst_pid++];
                                dst_off = 0;
                            }
                            for (int kk = 0; kk < kvsize; kk++) {
                                dst_page->buffer[dst_off + kk]
                                    = src_page->buffer[src_off + kk];
                            }
                        }
                        src_off += kvsize;
                        dst_off += kvsize;
                    }
                }
                if (src_page == dst_page && src_off == dst_off) {
                    dst_page = &pages[dst_pid++];
                    dst_off = 0;
                }
            }
            if (dst_page != NULL) dst_page->datasize = dst_off;
            pageid = dst_pid;
            pageoff = dst_off;
            while (dst_pid < pages.size()) {
                auto iter = pages.back();
                mem_aligned_free(iter.buffer);
                BaseDatabase<KeyType, ValType>::mem_bytes -= pagesize;
                pages.pop_back();
            }
            //this->slices.clear();
            std::unordered_map<char *, int> empty;
            this->slices.swap(empty);
            gbmem = 0;
        }
    }

    int64_t pagesize;

    size_t pageid;
    uint64_t pageoff;
    std::vector<Page> pages;

    char *ptr;
    int kvsize;

    int keycount, valcount;
    uint64_t kvcount;
    uint64_t kvmem;
    uint64_t gbmem;
    //uint32_t          bincount;

    int max_kvsize;
    int min_kvsize;

    bool ispointer;
    std::unordered_map<char *, int> slices;
    Serializer<KeyType, ValType> *ser;
};

} // namespace MIMIR_NS

#endif
