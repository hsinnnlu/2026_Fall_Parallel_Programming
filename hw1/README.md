# CS542200 Parallel Programming HW1：Odd-Even Sort（MPI）

NTHU 平行程式設計 HW1。用 MPI 實作 odd-even sort，資料要經過多輪 hashing，每一輪都要重新排序。

- 繳交程式：[`hw1.cc`](hw1.cc)
- 實驗程式與原始數據：[`test/`](test/)

## 編譯與執行

```bash
make                       # 等同 mpicxx -O3 -o hw1 hw1.cc，需要課程的 hash.h 放在 hw1/ 目錄
srun -n <procs> ./hw1 <n> <input_file> <output_file> [hash_rounds=25]
```

`hash.h` 是課程提供的檔案，不放進 repo（見 `.gitignore`）。

## 做法

所有 rank 用 MPI-IO 平均讀入一段資料，把 float 轉成可以直接比大小的 `uint32` key，之後每一輪：

1. **Local radix sort**：round 0 是 32-bit key（11+11+10 bit），之後 hash 輸出只有 24 bit（8+8+8 bit）。
2. **Odd-even phases + selective compare-split**：先交換邊界值，已經有序就跳過；否則只送出可能移動的元素，並且只 merge 出自己要留下的那一半。
3. **跨節點壓縮**：相鄰 rank 在不同節點時，送「和前一個數的差值」並用 varint 編碼。
4. **延後檢查終止**：前 p 個 phase 不做 `Allreduce`，之後才每個 cycle 檢查一次。

## 官方測試結果

總時間 **77.69 s**（40 筆測資，單位：秒）。

| 測資 | 時間 | 測資 | 時間 | 測資 | 時間 | 測資 | 時間 | 測資 | 時間 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 01 | 0.77 | 09 | 4.19 | 17 | 1.69 | 25 | 0.94 | 33 | 1.96 |
| 02 | 0.78 | 10 | 4.37 | 18 | 2.00 | 26 | 1.08 | 34 | 1.68 |
| 03 | 0.74 | 11 | 0.79 | 19 | 4.71 | 27 | 1.29 | 35 | 1.97 |
| 04 | 0.74 | 12 | 0.76 | 20 | 4.87 | 28 | 2.08 | 36 | 2.42 |
| 05 | 0.84 | 13 | 0.78 | 21 | 0.74 | 29 | 3.36 | 37 | 3.00 |
| 06 | 0.96 | 14 | 0.76 | 22 | 0.76 | 30 | 4.51 | 38 | 3.86 |
| 07 | 1.35 | 15 | 0.90 | 23 | 0.70 | 31 | 0.89 | 39 | 4.44 |
| 08 | 2.07 | 16 | 1.09 | 24 | 0.74 | 32 | 0.98 | 40 | 5.16 |

## 主要結果（測資 10，N = 23,987,513，25 rounds，5 次取中位數）

| 設定 | 最初版本 | 最終版本 |
|---|---:|---:|
| big，1 process | 42.40 s | **1.80 s**（約 23.5 倍） |
| big，4 processes | 15.80 s | 1.98 s |
| little，1 process | 61.60 s | 2.46 s |

| 跨節點（NFS） | 不壓縮 | 壓縮 |
|---|---:|---:|
| big，2 nodes × 4 | 9.71 s | **5.76 s** |
| mixed，2 nodes × 8 | 14.20 s | **5.86 s** |

完整數據在 [`test/exp/results/summary.csv`](test/exp/results/summary.csv)，圖在 [`test/exp/report/`](test/exp/report/)。

## 實驗（課程機器 Nova，在 `hw1/` 目錄下執行）

```bash
module load compiler/gcc/13 mpi/openmpi/5.0.10

# 1. 正式計時實驗：big / little / mixed / 2 節點（含跨節點壓縮開關），測資 10，每個設定 5 次
#    輸入輸出放 node-local /tmp；strong scaling（big 1/2/4 + 2 節點 8）全部放共享 NFS
bash test/exp/submit_all.sh

# 2. 優化的佐證：radix sort、selective compare-split、每 rank 至少 4096 筆
#    （跨節點壓縮的效果在步驟 1 的 big2n / mixed2n：final vs final_nocomp）
sbatch test/opt/run_opt.sh

# 3. Nsight Systems：只看 timeline 與通訊行為，不拿來計時
sbatch test/exp/nsys_job.sh
sbatch --export=ALL,VERSION=ori test/exp/nsys_job.sh

# ---- 全部跑完 ----
python3 test/exp/summarize.py      # → test/exp/results/summary.csv
python3 test/exp/make_report.py    # → test/exp/report/*.png 與 tables.tex
python3 test/exp/nsys_mpi.py test/exp/results/nsys/<資料夾>
python3 test/exp/analyze_input.py /srv/nova/scratch/coursedata/pp2026/hw1/10.in
```

## 檔案

| 路徑 | 用途 |
|---|---|
| `hw1.cc`、`Makefile` | 繳交程式與編譯 |
| `test/exp/prof_wrap.h` | PMPI 計時外掛（編譯時 `-include`，不改動 hw1.cc） |
| `test/exp/baseline_ori.cc` | 最初版本（前後比較用） |
| `test/exp/submit_all.sh`、`job.sh` | 編譯各版本並送出所有計時實驗 |
| `test/exp/summarize.py` | 整理結果（中位數、speedup；/tmp 與 NFS 分開） |
| `test/exp/make_report.py` | 產生報告的所有圖與 LaTeX 表格 |
| `test/exp/nsys_job.sh`、`nsys_mpi.py` | Nsight Systems profiling 與 timeline 圖 |
| `test/exp/analyze_input.py` | 測資數值分佈 |
| `test/opt/` | 三個優化的 micro-benchmark（`run_opt.sh`） |
| `test/parse_case.py`、`test/gen.py` | 讀測資設定、產生小測資 |
