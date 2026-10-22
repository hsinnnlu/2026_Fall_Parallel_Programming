#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

using ull = unsigned long long;
using u128 = unsigned __int128;

// 回傳有多少個 diagonal point：
// (x, x) 滿足 2*x*x < r*r
//
// 也剛好可以當成我們需要計算的 x 範圍上界。
static ull diagonal_count(ull r)
{
    u128 r2 = (u128)r * r;

    ull low = 0;
    ull high = r;

    while (low < high)
    {
        ull mid = low + (high - low) / 2;

        u128 value = 2 * (u128)mid * mid;

        if (value < r2)
            low = mid + 1;
        else
            high = mid;
    }

    return low;
}

// 精確計算 ceil(sqrt(value))
// sqrtl 只負責給初始估計，最後由 integer comparison 修正。
static ull ceil_sqrt_exact(ull value)
{
    ull y = (ull)sqrtl((long double)value);

    while (y * y < value)
        ++y;

    while (y > 0 && (y - 1) * (y - 1) >= value)
        --y;

    return y;
}

int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 3)
    {
        if (rank == 0)
            fprintf(stderr, "must provide exactly 2 arguments!\n");

        MPI_Finalize();
        return 1;
    }

    ull r = atoll(argv[1]);
    ull k = atoll(argv[2]);

    // 你的公開 testcase 最大是 2^32 - 1，
    // 因此 r*r 可以安全放在 unsigned long long。
    ull r2 = r * r;

    // D = diagonal points 數量
    //
    // 同時 x >= D 之後就已經在另一個對稱半邊，
    // 不需要繼續計算。
    ull diagonal = diagonal_count(r);

    // ==========================================
    // Equal partition of [0, diagonal)
    // ==========================================

    ull base = diagonal / size;
    ull remainder = diagonal % size;

    ull count =
        base + ((ull)rank < remainder ? 1 : 0);

    ull start =
        (ull)rank * base
        + ((ull)rank < remainder
            ? (ull)rank
            : remainder);

    ull end = start + count;

    // local_above =
    // 此 rank 負責的 y > x 點數
    ull local_above = 0;

    if (start < end)
    {
        ull d = r2 - start * start;

        // 每個 rank 只有這裡需要一次 sqrtl
        ull y = ceil_sqrt_exact(d);

        for (ull x = start; x < end; ++x)
        {
            if (x != start)
            {
                d = r2 - x * x;

                // 找到最小的 y，使得：
                // x*x + y*y >= r*r
                //
                // 等價於原本的
                // ceil(sqrt(r*r - x*x))
                while ((y - 1) * (y - 1) >= d)
                    --y;
            }

            // y 代表原本 ceil(sqrt(...))
            //
            // 可用的整數點：
            // 0, 1, ..., y-1
            //
            // 其中 > x 的數量：
            // y - x - 1
            local_above += y - x - 1;
        }
    }

    // ==========================================
    // Sum A from all ranks
    // ==========================================

    ull global_above = 0;

    MPI_Reduce(
        &local_above,
        &global_above,
        1,
        MPI_UNSIGNED_LONG_LONG,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );

    if (rank == 0)
    {
        // pixels = 2*A + D
        ull pixels =
            2 * global_above + diagonal;

        // 根據公開 testcase，pixels <= r*r < 2^64
        pixels %= k;

        printf("%llu\n", (4 * pixels) % k);
    }

    MPI_Finalize();

    return 0;
}
