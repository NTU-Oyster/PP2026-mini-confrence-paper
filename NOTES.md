# 進度日誌、設計決定、待決事項

## 待決事項（需要人類決定）

1. **核心數上限。** SEIREN 的 `hw` QoS 每個 job 最多 16 個邏輯 CPU（= 8 個實體核心 × 2 SMT）、
   32 GB、1 小時、同時 1 running + 2 queued。節點本身是 2 × Xeon Platinum 8352V（72 實體核心）。
   因此 RQ1/RQ2 目前只能做 1, 2, 4, 8（實體核心）與 16（SMT），做不到 CLAUDE.md 7.2 的 32+。
   帳號雖然也掛著 `judge` QoS（144 CPU），但那是給評分系統用的，**沒有使用**。
   若要更大的 p，請使用者向助教申請，或提供其他機器。
   RQ4（用 p ≤ 8 預測更大的 p）在此限制下只剩 p = 16 (SMT) 一個驗證點。
2. **GitHub repo 是公開的**（`NTU-Oyster/…`）。審稿是 double-blind，審稿結束前是否要 push、
   或先把 repo 轉為 private，請決定。目前只在本地 `dev` 分支 commit，未 push。
3. PPoPP2026 模板尚未放入 `paper/template/`（本機 TeX Live 有 `acmart.cls`，可暫用其 sigplan 格式）。
4. 組員分工（camera-ready 才需要）。
5. **第 6 節第 5 項的解讀需要確認。** 照字面（p = 1、同 seed、各自自由搜尋、N = 10^4，根節點平均值差 < 1e-4）
   **不會通過**：100 個狀態中 F32 有 95 個、FIX64 有 89 個超過 1e-4（價值差平均 1.4e-3 / 1.3e-3）。
   對照組：F64 只把每筆收益乘上 (1 + 2^-40)，也有 75 個超過（平均 1.3e-3）；F64 換 seed 是 98 個（平均 2.3e-3）。
   也就是任何極小的擾動都會讓搜尋軌跡分岔，之後的差異是 Monte Carlo 雜訊，不是累加誤差。
   目前測試改成「F64 決定選擇、同一條軌跡同時用各格式累加」，此時 F32 為 8.0e-6、FIX64 為 3.2e-7，通過 1e-4；
   BF16 為 2.7e-1（預期不會通過，只列印）。自由搜尋的數字照樣印出來。
   此改法請使用者同意；在此之前第 5 項視為「以修改後的準則通過」。
   推論：7.3 節「p = 1 比較各格式時只有精度不同」嚴格說是「精度 + 軌跡分岔」，所以 FP64 雜訊底線是必要的。

6. **【最重要，M5 前必須決定】tree-parallel 在原設計下沒有加速。** 見下方「校準結果」。
   每次迭代只要約 130 ns，而每次要對 root 附近的幾條 cache line 做數次原子 RMW，
   thread 數增加只會增加爭用。候選方案（不改 RQ）：
   - A：照原設計做，報告為負面結果（tree-parallel 不 scale；root/PIMC 有 6–7 倍）。
   - B（建議）：把「每個 leaf 的 rollout 數 m」當成新的實驗維度（m = 1, 8, 64；m = 1 就是原設計），
     用來掃描「爭用強度」，看不同格式的同步成本差異如何隨粒度改變。程式已支援（`--playouts`，預設 1）。
   - C：改成更大的樹（節點鍵含公共牌/資訊集）或更貴的 rollout，工程量較大，且會動到 4.2 節設計。
   在決定之前，M5 正式實驗不開跑。
7. **perf 不能用**（`perf_event_paranoid = 4`，一般使用者無權限），7.2 節的 `perf stat` 無法進行；
   除非助教調整，否則該項改為限制。

### 使用者決定（2026-11-25 第二輪）

- 待決 6 → **採方案 B**（m = 1, 8, 64）。待決 5 → **同意**修改後的準則。
- 待決 1（核心數）先不處理；待決 2（公開 repo）不用管；待決 7（perf）之後可能換環境再說。
- 待決 3 → 模板已收到（`paper/template/ppopp-template/`）。注意：它是 **PPoPP 2027** 版，
  預設不印行號且不允許 appendix；課程要求要有行號，論文階段以課程要求為準並再次確認 appendix。
- **一切程式只能透過 srun/sbatch 執行**；執行檔與腳本已加 Slurm 防呆（`src/util/slurm_guard.hpp`、`scripts/guard.sh`）。

## 環境事實（2026-11-25 實測）

- 計算節點：2 socket × 36 core × 2 SMT，Xeon Platinum 8352V，L2 1.25 MB/core，L3 54 MB/socket。
- gcc 15.2.0、cmake 4.2.3、OpenMP 4.5；`perf_event_paranoid = 4`，`perf stat` 可能無法使用（M4 確認）。
- 編譯與量測一律在 SEIREN 上透過 Slurm；本機只編輯與畫圖（SEIREN 沒有 numpy/matplotlib）。

## 文獻查證（`paper/refs.bib`）

- 清單外的重要相關研究：**Sephton, Cowling, Powley, Whitehouse, Slaven (CEC 2014),
  *Parallelization of Information Set Monte Carlo Tree Search***。比較 leaf/root/tree/tree+virtual loss 於 ISMCTS
  （遊戲 Lords of War），二手來源（MCTS 回顧論文）稱 root parallelization 最有效率。
  → 「平行化 ISMCTS」本身不是新的；新穎點必須限定在**數值格式 × 同步方式 × 爭用強度**。
  只查到摘要與二手摘要，沒有全文，寫 Related Work 時不可引述其細節數字。
- 另查到並加入：Mirsoleimani 2017（lock-free 樹 + virtual loss 的分析）、Schweizer 2015（x86 原子操作成本）。

## 設計決定

- **Hand evaluator 自行撰寫**（`src/game/hand_eval.hpp`），沒有第三方程式碼或查表檔，因此沒有授權問題。
  以每花色 13-bit mask 的位元運算實作；和 C(7,5) 窮舉的慢速版本比對。
- **籌碼單位**是半個 small bet（SB = 1, BB = 2, 下注 2/4）。大盲算 preflop 的第一注。
- **收益正規化**照 4.2 節除以理論最大底池 96（= 每人最多 48 × 2），所以實際值域是 [-0.5, 0.5]。
- **節點池大小上限**：從 preflop 開始的完整 open-loop 下注樹只有 16584 個節點
  （flop 2367、turn 261、river 27；不含對手手牌與公共牌，因為節點鍵只有公開動作序列）。
  10^5 以上的預算會把樹完全展開，所以 (a) 節點池不是記憶體瓶頸，整棵樹放得進 L2；
  (b) 越晚的街、樹越小，所有 thread 的更新集中在越少的節點上，**競爭反而更激烈**。
  這對 RQ1/RQ2 的解讀很重要：「格式越小 → cache 放得下越多節點」這條效應在 HULH 上預期很弱，
  照實報告即可，不改 RQ。

- **UCB1 的父節點次數**用剛讀到的子節點次數總和，不另設共享計數器，root 因此少一個熱點。
- **Virtual loss** 寫進收益欄位（選擇時加 ∓1，回傳時以 r ± 1 一次修正）；visit 在選擇時就加 1。
  序列基準有兩種：`serial_search<VL=true>` 和 tree-parallel p = 1 逐位元相同（測試用）；
  `VL=false` 才是量 speedup 用的純序列基準。
- **節點大小**（sizeof）：F64 系列、FIX64、F64_LOCK 都是 32 B，F32 與 BF16 是 24 B（8 B visit + 3 個 4 B 子索引已佔 20 B；
  F64_LOCK 的鎖剛好放進對齊空隙）。所以格式只造成 25% 的節點大小差異，BF16 不比 F32 小。
- **節點池**：共用一塊 calloc 空間，每個 thread 一次領 64 個節點（bump allocation），CAS 失敗的節點下次重用。

## M5 實驗規模（依校準調整，7.0 節）

- 分片清單：`scripts/worklist.py`，共 155 片，由 `scripts/run_queue.sh` 在 Slurm job 內續跑；
  全部固定在同一個節點（`-w hitoha`），每片的 load average 與 CPU 綁定記在 `results/raw/queue.log`。
  節點無法獨佔（帳號單一 job 上限 16 CPU），同節點若有他人 job 會造成雜訊，以 load average 監看。
- 固定「迭代數」而非「rollout 數」：m 不改變樹更新次數，只改變更新之間的間隔（= 爭用強度）。
- `rollouts_per_s` = 迭代數 × m / 時間。scaling 的 CSV 另加 `playouts, street, updates, lost_expansions` 欄。
- RQ3：參考答案 F64_CAS、p = 1、10^7、seed 100–102；測試 seed 0–2。
  p = 8 的 10^7 因成本只跑 250 個狀態、1 個 seed，其餘為 1000 個狀態、3 個 seed。
  RQ3 的固定迭代數部分只用 m = 1；固定時間部分（1/10/100 ms）涵蓋 m = 1, 8, 64。
- **UCB c = 0.25 定案**：4000 對 duplicate、20000 迭代，c = 0.1/0.5/1.0/2.0 對 c = 0.25 為
  -81 ± 58 / -65 ± 55 / -117 ± 80 / -419 ± 108 mbb/hand（`results/calib_c2.csv`）。
- PIMC 固定 K = 64 個世界（1–16 thread 的倍數）。

## 校準結果（2026-11-25，`results/calib.csv`、`results/calib_c.csv`、`results/diag/`）

- 單執行緒序列：10^6 次迭代約 0.131 s（約 7.6 M iter/s）；此環境、8 個狀態、1 個 seed。
- **tree-parallel 沒有加速**（`results/diag`，m = 1，8 個狀態、1 rep、budget 2×10^5，相對各自 p = 1）：
  p = 8 時 F64_CAS 0.70x、F64_LOCK 0.64x、FIX64 0.86x；root-parallel 6.2x。
  節點對齊到 64 B：F64_CAS 0.82x、FIX64 0.86x、F64_LOCK 0.69x，所以 false sharing 有影響但不是主因。
- 每個 leaf 做 m 次 rollout（總 rollout 數固定 2×10^5）：p = 8 的 F64_CAS 為
  m = 8 1.34x、m = 64 4.56x（對齊 64 B 則 1.57x、4.99x）；m = 64 時 root-parallel 為 6.0x，tree 各格式 4.4–4.6x。
  m = 64 的單次執行只有約 1 ms，這組數字只有 8 個狀態、1 rep，屬診斷等級，不可寫進論文。
- **CAS 重試**（m = 1，F64_CAS 在 10^6，`calib.csv`）：p = 2/4/8/16 約 0.06/0.12/0.19/0.52 次／更新。
- **UCB 常數**：對隨機/always-call/規則型 agent 在 c = 0.02–1.0 的掃描沒有單一最佳值，
  也沒有任何 c 在 4000 對牌局內打贏規則型 agent（-369 ~ -679 mbb/hand）。先用 0.25，正式值在 M5 前定案並記錄。
- **對規則型 agent 落敗不是已知的實作錯誤**：河牌面對下注時 19 個狀態的 call EV 與 990 手精確枚舉相符，
  25 個狀態的 fold EV 精確，高預算 ISMCTS（20000 迭代）對 200 迭代者 +486 ± 113（c = 0.25）mbb/hand，
  對 2000 迭代者 +68 ± 85（未達顯著）；c = 0.5 分別為 +919 ± 141 與 +221 ± 108。
  原因推測為單觀察者 + 均勻對手手牌、沒有依下注歷史做推論。Table 3 會照實呈現。

## 驗證結果

- M2/M3 的測試（`tests/test_search.cpp`）：Leduc 對隨機玩家 +1.568 ± 0.094、對 always-call +1.278 ± 0.044 chips/hand。
- TSan（`TSAN=1 scripts/build.sh`，以 std::thread 取代 OpenMP，因為 libgomp 未插樁）：`test_search` 0 個警告；
  負向對照 `tsan_negative_control`（刻意共享非原子樹）回報 19 個警告，證明偵測有效。
- SEIREN 登入節點的 cgroup 記憶體只有 300 MB，編譯會被 kill，所以編譯也要放進 `srun --mem=16G`。

## 日誌

- 2026-11-25 M1：遊戲規則、evaluator、Leduc、建置腳本與測試（第 6 節 1–2 項）。
- 2026-11-25 M4（部分）：狀態集、bench/quality/match 程式、環境與診斷腳本；正式腳本等待決事項 6。
- 2026-11-25 M2/M3：序列/tree/root/PIMC、五種格式、agents、第 6 節第 3–6 項（第 5 項準則見待決事項 5）。
