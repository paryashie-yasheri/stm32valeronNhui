// ══════════════════════════════════════════════════════════════════════════════
// report.typ — Typst-эквивалент LaTeX-преамбулы
//
// Воспроизводит стиль документа LaTeX (article + набор пакетов):
//
//   LaTeX                                     Typst
//   ───────────────────────────────────────   ─────────────────────────────────
//   \documentclass[a4paper,12pt]{article}      set page(paper: "a4"), set text(size: 12pt)
//   [T2A]{fontenc} + inputenc + babel{ru,en}   set text(lang: "ru", font: <Times>)
//   geometry(l=2, r=2, t=1, b=0.5 cm)          set page(margin: (...))
//   \pagestyle{empty}                          set page(numbering: none)
//   tempora + stix (Times)                     шрифт Liberation Serif
//   indentfirst + \parskip=0pt                 par(first-line-indent: .., spacing: leading)
//   \baselineskip=14.5pt (12pt)                par(leading: 0.5535em)
//   titlesec[pagestyles,raggedright]           show heading: set par(justify: false)
//   article.cls \Large/\large/\normalsize      размеры заголовков 1.44em/1.2em/1em
//   hyperref (linkcolor=black,urlcolor=blue)   show link: it => ...
//   \ref -> только номер                       show ref: set ref(supplement: none)
//   itemize/enumerate                          set list(marker:), set enum(numbering:)
//   booktabs                                   set table(stroke: none) + table.hline
//   caption                                    figure(caption: ..) (локализуется: «Рис.», «Таблица»)
//   minted                                     raw с указанием языка
//   \tableofcontents                           outline(title: [Содержание])
//
// Использование в основном файле:
//
//   #import "report.typ": report
//   #show: report
//   // … либо #show: report.with(style: "latex")
//
// Доступные стили:
//   "gost"  — ГОСТ 7.32-2017 (по умолчанию);
//   "latex" — точное повторение LaTeX-преамбулы.
// ══════════════════════════════════════════════════════════════════════════════

// Times-подобные шрифты (замена tempora/stix из LaTeX)
#let times-fonts = ("Liberation Serif", "Libertinus Serif", "Noto Serif")
#let mono-fonts = ("DejaVu Sans Mono", "Liberation Mono")

// Естественная высота строки Liberation Serif в Typst: 0.6548em.
// LaTeX при 12pt задаёт \baselineskip = 14.5pt = 1.2083em,
// отсюда leading = 1.2083em − 0.6548em = 0.5535em.
#let _natural-leading = 0.6548em

#let presets = (
  latex: (
    font: times-fonts,
    size: 12pt,
    margin: (left: 2cm, right: 2cm, top: 1cm, bottom: 0.5cm),   // geometry
    leading: 0.5535em,                                          // \baselineskip 14.5pt
    indent: 1.25em,                                             // \parindent 15pt
    numbering: none,                                            // \pagestyle{empty}
    heading-sizes: (1.44em, 1.2em, 1em),                        // \Large / \large / \normalsize
  ),
  gost: (
    font: times-fonts,
    size: 14pt,
    margin: (left: 3cm, right: 1.5cm, top: 2cm, bottom: 2cm),   // ГОСТ 7.32-2017
    leading: 1.5em - _natural-leading,                          // интервал 1,5
    indent: 1.25cm,                                             // абзацный отступ 5 знаков
    // нумерация сквозная снизу по центру, на титульном листе номер не ставится
    numbering: (..n) => if n.pos().at(0) == 1 { none } else { numbering("1", n.pos().at(0)) },
    heading-sizes: (1.2em, 1.1em, 1em),
  ),
)

#let report(
  body,
  style: "gost",
  font: none,
  size: none,
) = {
  let p = presets.at(style)
  let font = if font == none { p.font } else { font }
  let size = if size == none { p.size } else { size }

  // ── geometry + \pagestyle{empty} ───────────────────────────────────────────
  set page(
    paper: "a4",
    margin: p.margin,
    numbering: p.numbering,
    number-align: center,
  )

  // ── babel(russian,english) + шрифт (tempora/stix → Times) ──────────────────
  set text(font: font, size: size, lang: "ru", region: "RU")
  set par(
    justify: true,                                     // выключка по ширине
    leading: p.leading,                                // \baselineskip
    spacing: p.leading,                                // \parskip = 0pt (интервал в одну строку)
    first-line-indent: (amount: p.indent, all: true),  // indentfirst
  )

  // ── titlesec: заголовки по article.cls + raggedright ───────────────────────
  // Отступы заданы в pt (Typst не поддерживает ex); 1ex ≈ 0.45em для Times,
  // при 12pt это 5.4pt: \section 3.5ex/2.3ex, \subsection 3.25ex/1.5ex.
  let k = size / 12pt
  set heading(numbering: "1.1")
  show heading: set par(justify: false)
  show heading.where(level: 1): set text(size: p.heading-sizes.at(0), weight: "bold")
  show heading.where(level: 2): set text(size: p.heading-sizes.at(1), weight: "bold")
  show heading.where(level: 3): set text(size: p.heading-sizes.at(2), weight: "bold")
  show heading.where(level: 1): set block(above: 18.9pt * k, below: 12.4pt * k)
  show heading.where(level: 2): set block(above: 17.6pt * k, below: 8.1pt * k)
  show heading.where(level: 3): set block(above: 17.6pt * k, below: 8.1pt * k)

  // ── hyperref: linkcolor=black, urlcolor=blue; \ref → только номер ─────────
  show link: it => if type(it.dest) == str {
    text(fill: rgb("#0000ff"), it.body)
  } else {
    text(fill: black, it.body)
  }
  show ref: set ref(supplement: none)

  // ── itemize/enumerate (article.cls) ────────────────────────────────────────
  set list(marker: ([•], [–], [∗]))
  set enum(numbering: "1.")

  // ── booktabs: линейки задаются явно через table.hline ──────────────────────
  set table(stroke: none, inset: (x: 6pt, y: 4pt))

  // ── minted: моноширинный код ───────────────────────────────────────────────
  show raw: set text(font: mono-fonts, size: 0.9em)

  // ── \tableofcontents ───────────────────────────────────────────────────────
  set outline(depth: 3)

  body
}
