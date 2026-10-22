# Lab2：Pixels in Circle（Pthread / OpenMP / Hybrid）

同 Lab1 的題目，分別用三種方式平行化。

| 檔案 | 平行化方式 | 編譯 |
|---|---|---|
| `lab2_pthread.cc` | Pthread | `g++ -O3 -pthread -o lab2_pthread lab2_pthread.cc` |
| `lab2_omp.cc` | OpenMP | `g++ -O3 -fopenmp -o lab2_omp lab2_omp.cc` |
| `lab2_hybrid.cc` | MPI + OpenMP | `mpicxx -O3 -fopenmp -o lab2_hybrid lab2_hybrid.cc` |

執行：`./lab2_pthread <r> <k>`、`./lab2_omp <r> <k>`、`srun -n <procs> -c <threads> ./lab2_hybrid <r> <k>`

## 做法

三個版本共用同一個核心迴圈：

- **可向量化的 ceil(sqrt)**：不用 `ceil()`（GCC 會變成無法向量化的呼叫），改成截斷取整再用整數比較校正成精確值；`r² − x²` 可能超過 2⁶³，先右移 1 位轉成 signed 再轉 double，讓 GCC 能用 AVX-512 向量化。
- **分區塊取模**：每 2¹⁶ 個 x 一個區塊，區塊內直接累加，結束才 `% k`，避免溢位又減少取模次數。
- **平行化**：
  - Pthread：依 `sched_getaffinity` 取得可用 CPU 數開 thread，每條 thread 負責一段連續的 x。
  - OpenMP：區塊用 `omp for schedule(static)` 分給 thread，內層用 `omp simd reduction`。
  - Hybrid：先依 x 的個數切給各 MPI rank，rank 內再用 OpenMP 分區塊；跨 rank 用自訂的 `(a + b) % k` MPI_Op 做 `MPI_Reduce`，避免加總溢位。

## 官方測試結果（12 筆測資，單位：秒）

**Pthread**：總時間 **5.23 s**

| 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 10 | 11 | 12 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0.41 | 0.41 | 0.35 | 0.36 | 0.34 | 0.37 | 0.36 | 0.39 | 0.35 | 0.63 | 0.53 | 0.73 |

**OpenMP**：總時間 **5.40 s**

| 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 10 | 11 | 12 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0.43 | 0.41 | 0.35 | 0.36 | 0.36 | 0.37 | 0.34 | 0.36 | 0.35 | 0.63 | 0.51 | 0.93 |

**Hybrid（MPI + OpenMP）**：總時間 **8.65 s**

| 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 10 | 11 | 12 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0.75 | 0.66 | 0.68 | 0.71 | 0.69 | 0.66 | 0.68 | 0.68 | 0.66 | 0.83 | 0.73 | 0.91 |
