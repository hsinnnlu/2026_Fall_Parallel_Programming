#pragma GCC target("avx512f,avx512dq")
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <cstdlib>

struct ThreadData {
        int id;
        int nthreads;
        unsigned long long r;
        unsigned long long k;
        unsigned long long result;
};

// 每個區塊的長度：區塊內先單純累加，結束後才取模，避免溢位
const unsigned long long BLOCK = 1ULL << 16;

void* worker(void* arg) {
        ThreadData* d = (ThreadData*)arg;
        unsigned long long r = d->r;
        unsigned long long k = d->k;
        unsigned long long r2 = r * r;

        unsigned long long chunk = (r + d->nthreads - 1) / d->nthreads;
        unsigned long long start = (unsigned long long)d->id * chunk;
        unsigned long long end = start + chunk < r ? start + chunk : r;

        unsigned long long pixels = 0;

        for (unsigned long long bs = start; bs < end; bs += BLOCK) {
                unsigned long long be = bs + BLOCK < end ? bs + BLOCK : end;
                unsigned long long sum = 0;

                // 迴圈內沒有分支、每個 x 互相獨立 → GCC 可以自動向量化
                for (unsigned long long x = bs; x < be; x++) {
                        unsigned long long rem = r2 - x * x;
                        // rem 可能超過 2^63，先右移 1 位轉成 signed 再轉 double（GCC 只能向量化 signed 轉換）
                        double rem_d = (double)(long long)(rem >> 1) * 2.0;
                        // 不用 ceil()：(long long)ceil() 會被 GCC 變成無法向量化的 llceil 呼叫
                        // 改用截斷取整，再用整數比較校正成精確的 ceil(sqrt(rem))
                        unsigned long long y = (long long)sqrt(rem_d);
                        y += (y * y < rem);
                        y += (y * y < rem);
                        y -= (y > 0) & ((y - 1) * (y - 1) >= rem);
                        sum += y;
                }

                pixels = (pixels + sum % k) % k;
        }

        d->result = pixels;
        return NULL;
}

int main(int argc, char** argv) {
        if (argc != 3) {
                fprintf(stderr, "must provide exactly 2 arguments!\n");
                return 1;
        }

        unsigned long long r = strtoull(argv[1], NULL, 10);
        unsigned long long k = strtoull(argv[2], NULL, 10);

        cpu_set_t cpuset;
        sched_getaffinity(0, sizeof(cpuset), &cpuset);
        int ncpus = CPU_COUNT(&cpuset);
        if (ncpus < 1) ncpus = 1;

        ThreadData* data = new ThreadData[ncpus];
        pthread_t* threads = new pthread_t[ncpus];

        for (int i = 0; i < ncpus; i++) {
                data[i].id = i;
                data[i].nthreads = ncpus;
                data[i].r = r;
                data[i].k = k;
                pthread_create(&threads[i], NULL, worker, &data[i]);
        }

        unsigned long long total = 0;
        for (int i = 0; i < ncpus; i++) {
                pthread_join(threads[i], NULL);
                total = (total + data[i].result) % k;
        }

        printf("%llu\n", (4 * total) % k);

        delete[] data;
        delete[] threads;
        return 0;
}
