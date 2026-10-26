// ═════════════════════════════════════════════════════════════════════════════
// title-page-template.typ — титульный лист (функция title-page)
//
// Оформление по ГОСТ 7.32-2017 (п. 6.10):
//   • все реквизиты — тем же кеглем, что и основной текст (без разных размеров);
//   • реквизиты набирают через один межстрочный интервал;
//   • полужирный — только для заголовков;
//   • место и год — по центру в нижней части листа;
//   • номер страницы на титульном листе не проставляют (нумерация сквозная).
//
// Используется вместе со стилем report.typ:
//
//   #import "report.typ": report
//   #import "title-page-template.typ": title-page
//   #show: report
//
//   #title-page(subject: "…", work-number: "1", …)
//   #pagebreak()
// ═════════════════════════════════════════════════════════════════════════════

#let title-page(
  subject: "Компьютерные сети",
  work-type: "Лабораторная работа",
  work-number: "1",
  student-name: "Фамилия И.О.",
  student-group: "Р3315",
  reviewer-name: "Преподаватель",
  city: "Санкт-Петербург",
  year: "2026",
  ministry-line: "Министерство образования и науки Российской Федерации",
  university: "федеральное государственное автономное образовательное учреждение высшего образования",
  university-short: "НАЦИОНАЛЬНЫЙ ИССЛЕДОВАТЕЛЬСКИЙ УНИВЕРСИТЕТ ИТМО",
  faculty: "Факультет «Программной инженерии и компьютерной техники»",
) = {
  // один межстрочный интервал; кегль наследуется от основного текста
  set text(hyphenate: false)
  set par(leading: 0.55em, spacing: 0.55em)

  block(height: 100%)[
    #align(center)[
      #ministry-line \
      #university \
      #university-short

      #v(1.5em)

      #faculty
    ]

    #v(3cm)

    #align(center)[
      #subject

      #v(1em)

      #work-type №#work-number
    ]

    #v(4cm)

    #align(right)[
      #text(weight: "bold")[Выполнил:] \
      #student-name \
      Группа: #student-group

      #v(1.5em)

      #text(weight: "bold")[Проверил:] \
      #reviewer-name
    ]

    #v(1fr)

    #align(center)[#city, #year]
  ]
}