#pragma GCC target("avx512f,avx512dq")
#include <stdio.h>
#include <math.h>
#include <mpi.h>
#include <stdlib.h>
#include <omp.h>

// 區塊長度上限：區塊內先單純累加，結束後才取模，避免溢位
const unsigned long long MAX_BLOCK = 1ULL << 16;

unsigned long long k;

// 自訂 MPI 運算：(a + b) % k，避免 MPI_SUM 加總後溢位
void mod_add(void* in, void* inout, int* len, MPI_Datatype* dtype) {
        unsigned long long* a = (unsigned long long*)in;
        unsigned long long* b = (unsigned long long*)inout;
        for (int i = 0; i < *len; i++) {
                b[i] = (a[i] + b[i]) % k;
        }
}

int main(int argc, char** argv) {
        MPI_Init(&argc, &argv);

        int rank, size;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);

        if (argc != 3) {
                if (rank == 0) fprintf(stderr, "must provide exactly 2 arguments!\n");
                MPI_Finalize();
                return 1;
        }

        unsigned long long r = strtoull(argv[1], NULL, 10);
        k = strtoull(argv[2], NULL, 10);
        unsigned long long r2 = r * r;

        // MPI 層：按 x 的個數切給各個 rank，每個 rank 的數量最多差 1
        unsigned long long base = r / size, extra = r % size;
        unsigned long long xs = rank * base + (rank < extra ? rank : extra);
        unsigned long long xe = xs + base + (rank < extra ? 1 : 0);

        unsigned long long pixels = 0;

        #pragma omp parallel
        {
                // 區塊只負責取模，大小依執行緒數調整，讓每條執行緒都分得到工作
                unsigned long long len = xe - xs;
                unsigned long long nthreads = omp_get_num_threads();
                unsigned long long block = (len + nthreads - 1) / nthreads;
                if (block > MAX_BLOCK) block = MAX_BLOCK;
                if (block < 1) block = 1;
                unsigned long long nblocks = (len + block - 1) / block;

                unsigned long long local = 0;

                // OpenMP 層：rank 內的區塊再分給各條執行緒
                #pragma omp for schedule(static) nowait
                for (unsigned long long b = 0; b < nblocks; b++) {
                        unsigned long long bs = xs + b * block;
                        unsigned long long be = bs + block < xe ? bs + block : xe;
                        unsigned long long sum = 0;

                        // SIMD 層：區塊內的 x 用向量指令一起算
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

                #pragma omp critical
                pixels = (pixels + local) % k;
        }

        MPI_Op op_mod_add;
        MPI_Op_create(mod_add, 1, &op_mod_add);

        unsigned long long total = 0;
        MPI_Reduce(&pixels, &total, 1, MPI_UNSIGNED_LONG_LONG, op_mod_add, 0, MPI_COMM_WORLD);

        if (rank == 0) {
                printf("%llu\n", (4 * total) % k);
        }

        MPI_Op_free(&op_mod_add);
        MPI_Finalize();
        return 0;
}
