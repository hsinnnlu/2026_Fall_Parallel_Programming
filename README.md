# 2026 Fall Parallel Programming（CS542200, NTHU）

清華大學 2026 秋季「平行程式設計」課程作業。

| 作業 | 主題 | 內容 | 官方測試總時間 |
|---|---|---|---:|
| [Lab1](lab1/) | Pixels in Circle（MPI） | 只算八分之一圓、增量更新 y 避免重複 sqrt | 6.69 s |
| [Lab2](lab2/) | Pixels in Circle（Pthread / OpenMP / Hybrid） | AVX-512 向量化的精確 ceil(sqrt)、分區塊取模 | 5.23 / 5.40 / 8.65 s |
| [HW1](hw1/) | Odd-Even Sort（MPI） | 多輪 hashing 的 odd-even sort：radix sort、selective compare-split、跨節點壓縮 | 77.69 s |

每份作業放在自己的資料夾（`lab1/`、`hw1/`…），各自有 README 說明編譯方式與實驗。
課程提供的檔案（如 `hash.h`）不放進 repo。
