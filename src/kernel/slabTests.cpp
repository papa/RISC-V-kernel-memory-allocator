#include "../../h/slabTests.hpp"
#include "../../h/KConsole.hpp"

void slabInitTest()
{
    kmem_init((void*)HEAP_START_ADDR, 15);
}

void slabCacheCreateTest()
{
    kmem_init((void*)HEAP_START_ADDR, 15);
    kmem_cache_t* cache = kmem_cache_create("Cache test", 10, nullptr,nullptr);
    kmem_cache_t* cache2 = kmem_cache_create("Cache test 2", 10, nullptr,nullptr);
    kmem_cache_t* cache3 = kmem_cache_create("Cache test 3", 10, nullptr,nullptr);
    if(cache && cache2 && cache3)
        printSlabAllocatorInfo();
}

void slabCacheCreateTest2()
{
    kmem_init((void*)HEAP_START_ADDR, 15);
    kmem_cache_t * cache = nullptr;
    for(int i = 1;i<=68;i++)
    {
        cache = kmem_cache_create("Cache test", 10, nullptr,nullptr);
    }
    printSlabAllocatorInfo();
    kmem_cache_destroy(cache);
    printSlabAllocatorInfo();
}

void testKmalloc()
{
    void** x = (void**)kmalloc(sizeof(void*)*200);
    int indeks = 0;
    for(int i = 0;i < 10;i++)
    {
        for(int j = 2;j<=11;j++)
        {
            void* adr = kmalloc(1 << j);
            x[indeks++] = adr;
            if(adr == nullptr)
            {
                KConsole::trapPrintString("Nullptr je\n");
                KConsole::trapPrintStringInt("i ", i);
                KConsole::trapPrintStringInt("j ", j);
            }
        }
    }
    for(int i = 0; i < indeks;i++)
    {
      kfree(x[i]);
    }
    printSlabAllocatorInfo();
}

void testKmalloc2()
{
    bool flag = false;
    for(int i = 0;!flag;i++)
    {
        for(int j = 5;j<=17;j++)
        {
            void* adr = kmalloc(1 << j);
            if(adr == nullptr)
            {
                KConsole::trapPrintString("Nullptr je\n");
                KConsole::trapPrintStringInt("i ", i);
                KConsole::trapPrintStringInt("j ", j);
                flag = true;
                break;
            }
        }
    }
}

//-----------------------------------------------------------------------------------------------------------------------
//OS2 testovi

#define RUN_NUM (5)
#define ITERATIONS (1000)

#define shared_size (7)
#define MASK (0xA5)

struct data_s {
    int id;
    kmem_cache_t  *shared;
    int iterations;
};

const char * const CACHE_NAMES[] = {"tc_0",
                                    "tc_1",
                                    "tc_2",
                                    "tc_3",
                                    "tc_4"};

void memset(const void *data, int size, int value) {
    for (int j = 0; j < size; j++) {
        *((char *)data + j) = value;
    }
}

void construct(void *data) {
    //static int i = 1;
    //KConsole::trapPrintInt(i); i++;
    //KConsole::trapPrintString(" Shared object constructed.\n");
    memset(data, shared_size, MASK);
}

int check(void *data, size_t size) {
    int ret = 1;
    for (size_t i = 0; i < size; i++) {
        if (((unsigned char *)data)[i] != MASK) {
            ret = 0;
        }
    }

    return ret;
}

struct objects_s {
    kmem_cache_t *cache;
    void *data;
};

void work(void* pdata)
{
    struct data_s data = *(struct data_s*) pdata;
    int size = 0;
    int object_size = data.id + 1;
    kmem_cache_t *cache = kmem_cache_create(CACHE_NAMES[data.id], object_size, 0, 0);

    struct objects_s *objs = (struct objects_s*)(kmalloc(sizeof(struct objects_s) * data.iterations));

    for (int i = 0; i < data.iterations; i++) {
        if (i % 100 == 0) {
            objs[size].data = kmem_cache_alloc(data.shared);
            objs[size].cache = data.shared;
            if (!check(objs[size].data, shared_size)) {
                KConsole::trapPrintString("Value not correct!");
            }
        }
        else {
            objs[size].data = kmem_cache_alloc(cache);
            objs[size].cache = cache;
            memset(objs[size].data, object_size, MASK);
        }
        size++;
    }

    kmem_cache_info(cache);
    kmem_cache_info(data.shared);
    //printSlabAllocatorInfo();

    for (int i = 0; i < size; i++) {
        if (!check(objs[i].data, (cache == objs[i].cache) ? object_size : shared_size)) {
            KConsole::trapPrintString("Value not correct!");
        }
        kmem_cache_free(objs[i].cache, objs[i].data);
    }
    kmem_cache_info(data.shared);
    //kmem_cache_info(cache);

    //kmem_cache_info(cache);
    //kmem_cache_info(data.shared);
    //printSlabAllocatorInfo();

    kfree(objs);
    kmem_cache_destroy(cache);

    //printSlabAllocatorInfo();
}

void runs(void(*work)(void*), struct data_s* data, int num) {
    for (int i = 0; i < RUN_NUM; i++) {
        struct data_s private_data;
        private_data = *(struct data_s*) data;
        private_data.id = i;
        work(&private_data);
    }
}

void testOS2()
{
    kmem_cache_t *shared = kmem_cache_create("shared object", shared_size, construct, nullptr);

    struct data_s data;
    data.shared = shared;
    data.iterations = ITERATIONS;
    runs(work, &data, RUN_NUM);

    kmem_cache_destroy(shared);
}