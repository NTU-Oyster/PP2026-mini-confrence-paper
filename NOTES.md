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

## 日誌

- 2026-11-25 M1：遊戲規則、evaluator、Leduc、建置腳本與測試（第 6 節 1–2 項）。
