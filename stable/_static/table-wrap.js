/*
 * Article helpers for inline code and tables (progressive enhancement; the
 * pages read correctly without them). The file name is historical.
 *
 * 1. Long inline code (prose and tables). A code span of 20+ characters, or
 *    of 12+ characters containing "_", "/", "." or ":", gets class "wrap-ok"
 *    and a <wbr> after each of those characters, so paths and identifiers
 *    such as parameters/core/parameters_vesicle_quick.xml wrap at word
 *    boundaries on a phone. Short tokens such as 2.5e-8 are left alone and
 *    never wrap (custom.css lets only .wrap-ok spans wrap).
 * 2. Numeric columns. A column (header included) gets class "num" on every
 *    cell (right-aligned, tabular figures, no line break; see custom.css)
 *    only when every non-empty body cell in it is a number such as 1e-7,
 *    49,748, 39%, 2.0x (with the multiplication sign), 2.53 ± 0.06 or ~145,000.
 *    Columns with any text cell stay left-aligned throughout.
 * 3. Prose columns. A column whose non-empty body cells average 40 or more
 *    characters gets class "prose"; on a phone only these columns keep a wide
 *    minimum width.
 *
 * No regex lookbehind: it is a parse error in Safari before 16.4.
 */
(function () {
  "use strict";
  var N = "[~\u2248<>\u2264\u2265]?\\s?[\u2212+\\-]?\\d[\\d,\u202f\u2009]*(?:\\.\\d+)?(?:[eE][\u2212+\\-]?\\d+)?";
  var NUM = new RegExp("^" + N + "(?:\\s?\u00b1\\s?" + N + ")?\\s?(?:%|\u00d7)?$");
  var EMPTY = /^(|-|\u2013|\u2014)$/;

  var BREAK_AFTER = /([_/.:])/g;

  function addBreaks(root) {
    root.querySelectorAll("code span.pre").forEach(function (span) {
      if (span.closest("pre")) return;
      if (span.childNodes.length !== 1 || span.firstChild.nodeType !== 3) return;
      var text = span.textContent;
      var long = text.length >= 20 || (text.length >= 12 && /[_/.:]/.test(text));
      if (!long) return;
      span.classList.add("wrap-ok");
      var parts = text.replace(BREAK_AFTER, "$1\u0000").split("\u0000");
      span.textContent = "";
      parts.forEach(function (part, i) {
        if (part === "") return;
        if (i > 0) span.appendChild(document.createElement("wbr"));
        span.appendChild(document.createTextNode(part));
      });
    });
  }

  // Column index of each cell, counting colspans.
  function columnsOf(row) {
    var cols = [], c = 0;
    Array.prototype.forEach.call(row.cells, function (cell) {
      cols.push(c);
      c += cell.colSpan || 1;
    });
    return cols;
  }

  function markNumbers(table) {
    var body = table.tBodies[0];
    if (!body) return;
    var numeric = {};  // column -> true while every non-empty cell is a number
    var seen = {};     // column -> at least one number
    Array.prototype.forEach.call(body.rows, function (row) {
      var cols = columnsOf(row);
      Array.prototype.forEach.call(row.cells, function (cell, i) {
        var col = cols[i];
        if ((cell.colSpan || 1) > 1) { numeric[col] = false; return; }
        var text = cell.textContent.replace(/\s+/g, " ").trim();
        if (EMPTY.test(text)) return;
        if (numeric[col] === undefined) numeric[col] = true;
        if (NUM.test(text)) seen[col] = true; else numeric[col] = false;
      });
    });
    var rows = Array.prototype.slice.call(body.rows);
    if (table.tHead) rows = rows.concat(Array.prototype.slice.call(table.tHead.rows));
    rows.forEach(function (row) {
      var cols = columnsOf(row);
      Array.prototype.forEach.call(row.cells, function (cell, i) {
        if (numeric[cols[i]] && seen[cols[i]] && (cell.colSpan || 1) === 1) cell.classList.add("num");
      });
    });
  }

  function markProse(table) {
    var body = table.tBodies[0];
    if (!body) return;
    var total = {}, count = {};
    Array.prototype.forEach.call(body.rows, function (row) {
      var cols = columnsOf(row);
      Array.prototype.forEach.call(row.cells, function (cell, i) {
        var text = cell.textContent.replace(/\s+/g, " ").trim();
        if (EMPTY.test(text) || (cell.colSpan || 1) > 1) return;
        total[cols[i]] = (total[cols[i]] || 0) + text.length;
        count[cols[i]] = (count[cols[i]] || 0) + 1;
      });
    });
    Array.prototype.forEach.call(body.rows, function (row) {
      var cols = columnsOf(row);
      Array.prototype.forEach.call(row.cells, function (cell, i) {
        var c = cols[i];
        if (count[c] && total[c] / count[c] >= 40) cell.classList.add("prose");
      });
    });
  }

  function run() {
    var article = document.querySelector(".bd-article");
    if (!article) return;
    addBreaks(article);
    article.querySelectorAll("table").forEach(function (table) {
      markNumbers(table);
      markProse(table);
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", run);
  } else {
    run();
  }
})();
