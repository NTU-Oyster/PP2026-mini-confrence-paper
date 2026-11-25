# PPoPP 論文 LaTeX 範本

ACM SIGPLAN 雙欄格式（`acmart` + `sigplan` + `10pt`），依 PPoPP 2027 Call for Papers 規範設定。
內附 acmart v2.20（2026/08/16），不需另外下載 ACM 官方的 zip。

## 檔案結構

```
main.tex                  主檔：模式切換、會議資訊、作者、摘要
sections/                 各章節內容（主要在這裡寫）
  abstract.tex
  introduction.tex
  background.tex
  design.tex
  evaluation.tex
  related.tex
  conclusion.tex
figures/                  圖檔（建議 PDF 向量圖）
references.bib            參考文獻
acmart.cls                ACM 文件類別（請勿修改）
ACM-Reference-Format.bst  ACM 參考文獻格式（請勿修改）
latexmkrc, Makefile       編譯設定
```

## 開始使用

**Overleaf（推薦）**：New Project → Upload Project → 選這個 zip。編譯器使用預設的 pdfLaTeX，主檔是 `main.tex`。

**本機**：需要 TeX Live 2023 以上或 MacTeX，在資料夾內執行 `make`（或 `latexmk -pdf main.tex`）。

## 投稿版與最終版

`main.tex` 開頭只需改一行：

```latex
\submissiontrue      % 投稿版：匿名、有頁碼、無行號、不印 CCS / ACM Reference
%\submissionfalse    % camera-ready：錄取後使用
```

投稿版不使用 `review` 選項，因為它會在頁邊加行號，可能觸發 HotCRP 的格式檢查。作者資訊請照常填寫，`anonymous` 選項會自動隱藏，致謝（`acks`）也會自動隱藏。

錄取後切換成 `\submissionfalse`，並把 ACM rights 確認信中的 `\setcopyright`、`\acmBooktitle`、`\acmISBN`、`\acmDOI` 換成正確內容。PPoPP 2027 起全面 Open Access，授權為 CC-BY 或 CC-BY-NC-ND 二選一（範本預設 CC-BY，用 `\setcctype{by-nc-nd}` 切換）。

## PPoPP 格式規範摘要

| 項目 | 規定 |
|---|---|
| 格式 | acmart、sigplan、10pt、雙欄 |
| 頁數 | 正文含圖表最多 10 頁；參考文獻不計頁、不限長度 |
| 摘要 | 100–400 字 |
| Appendix | 不允許；證明、程式碼等以補充資料（PDF/ZIP）另外上傳 |
| 審查方式 | 雙盲 |
| 字級 | 圖說 ≥ 9pt；圖表內文字 ≥ 8pt |
| 紙張 | PDF 需能以 A4 與 US Letter 列印 |

會議規範每年可能微調，投稿前請以當年官網 Call for Papers 為準。

## 投稿前檢查清單

**版面（違反會被 desk reject）**
- [ ] 沒有用 `\vspace`（負值）、`\baselinestretch`、`geometry`、`\setlength{\columnsep}`、`\titlespacing` 等方式擠空間
- [ ] 沒有修改 `acmart.cls`，也沒有用 `\small`/`\footnotesize` 縮小整段正文
- [ ] 正文（不含 References）不超過 10 頁
- [ ] 沒有 `\appendix`
- [ ] 表格與圖片內文字不小於 8pt（不要把大圖縮小塞進單欄）

**雙盲**
- [ ] PDF 首頁顯示 "Anonymous Author(s)"
- [ ] 引用自己的舊作時用第三人稱（"Smith et al. proposed…" 而非 "In our prior work…"）
- [ ] 正文、圖片、程式碼連結中沒有作者姓名、學校、實驗室名稱或 GitHub 帳號（Artifact 連結請用匿名服務，如 anonymous.4open.science）
- [ ] PDF metadata 沒有作者名稱（`pdfinfo main.pdf` 檢查 Author 欄位）

**參考文獻**
- [ ] 列出所有作者，不可用 `and others` 產生 et al.
- [ ] 每筆都有年份與會議/期刊名稱，不能只有 DOI 或網址

**其他**
- [ ] PDF 中沒有紅色 `[TODO: …]`
- [ ] 若使用生成式 AI 協助撰寫文字、程式或圖表，已在 Acknowledgments 揭露（camera-ready 時會顯示）

## 常見問題

**可以加 `\usepackage{...}` 嗎？** 一般功能套件（`algorithm2e`、`tikz`、`pgfplots`、`multirow` 等）可以。不要載入會改版面的套件，例如 `geometry`、`fullpage`、`times`、`titlesec`、`setspace`。

**畫圖建議？** 用 matplotlib 時直接把 `figsize` 設成刊出尺寸（單欄約 3.33 吋寬、雙欄約 7 吋），字級設 8pt 以上，存成 PDF。範例圖 `figures/speedup.pdf` 就是這樣產生的。

**跨欄圖表？** 用 `figure*` / `table*`，只會出現在頁面上方或下一頁。
