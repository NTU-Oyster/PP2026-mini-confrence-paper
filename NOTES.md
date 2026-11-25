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

## 環境事實（2026-11-25 實測）

- 計算節點：2 socket × 36 core × 2 SMT，Xeon Platinum 8352V，L2 1.25 MB/core，L3 54 MB/socket。
- gcc 15.2.0、cmake 4.2.3、OpenMP 4.5；`perf_event_paranoid = 4`，`perf stat` 可能無法使用（M4 確認）。
- 編譯與量測一律在 SEIREN 上透過 Slurm；本機只編輯與畫圖（SEIREN 沒有 numpy/matplotlib）。

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

## 驗證結果

- M2/M3 的測試（`tests/test_search.cpp`）：Leduc 對隨機玩家 +1.568 ± 0.094、對 always-call +1.278 ± 0.044 chips/hand。
- TSan（`TSAN=1 scripts/build.sh`，以 std::thread 取代 OpenMP，因為 libgomp 未插樁）：`test_search` 0 個警告；
  負向對照 `tsan_negative_control`（刻意共享非原子樹）回報 19 個警告，證明偵測有效。
- SEIREN 登入節點的 cgroup 記憶體只有 300 MB，編譯會被 kill，所以編譯也要放進 `srun --mem=16G`。

## 日誌

- 2026-11-25 M1：遊戲規則、evaluator、Leduc、建置腳本與測試（第 6 節 1–2 項）。
- 2026-11-25 M2/M3：序列/tree/root/PIMC、五種格式、agents、第 6 節第 3–6 項（第 5 項準則見待決事項 5）。
