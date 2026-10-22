#pragma GCC target("avx512f,avx512dq")
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <omp.h>

// 區塊長度上限：區塊內先單純累加，結束後才取模，避免溢位
const unsigned long long MAX_BLOCK = 1ULL << 16;

int main(int argc, char** argv) {
        if (argc != 3) {
                fprintf(stderr, "must provide exactly 2 arguments!\n");
                return 1;
        }
        unsigned long long r = strtoull(argv[1], NULL, 10);
        unsigned long long k = strtoull(argv[2], NULL, 10);

        unsigned long long r2 = r * r;
        unsigned long long pixels = 0;

        #pragma omp parallel
        {
                // 區塊只負責取模，大小依執行緒數調整，讓每條執行緒都分得到工作
                unsigned long long nthreads = omp_get_num_threads();
                unsigned long long block = (r + nthreads - 1) / nthreads;
                if (block > MAX_BLOCK) block = MAX_BLOCK;
                if (block < 1) block = 1;
                unsigned long long nblocks = (r + block - 1) / block;

                unsigned long long local = 0;

                // 切工作交給 OpenMP：每條執行緒自動分到一段連續的區塊
                #pragma omp for schedule(static) nowait
                for (unsigned long long b = 0; b < nblocks; b++) {
                        unsigned long long bs = b * block;
                        unsigned long long be = bs + block < r ? bs + block : r;
                        unsigned long long sum = 0;

                        // 向量化也交給 OpenMP：omp simd 告訴編譯器這個迴圈可以用 SIMD
                        #pragma omp simd reduction(+:sum)
                        for (unsigned long long x = bs; x < be; x++) {
                                unsigned long long rem = r2 - x * x;
                                // rem 可能超過 2^63，先右移 1 位轉成 signed 再轉 double
                                double rem_d = (double)(long long)(rem >> 1) * 2.0;
                                // 截斷取整後用整數比較校正成精確的 ceil(sqrt(rem))
                                unsigned long long y = (long long)sqrt(rem_d);
                                y += (y * y < rem);
                                y += (y * y < rem);
                                y -= (y > 0) & ((y - 1) * (y - 1) >= rem);
                                sum += y;
                        }

                        local = (local + sum % k) % k;
                }

                // 合併各執行緒的結果（每條執行緒只進來一次）
                #pragma omp critical
                pixels = (pixels + local) % k;
        }

        printf("%llu\n", (4 * pixels) % k);

        return 0;
}
