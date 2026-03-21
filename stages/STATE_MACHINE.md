# `mycsv_parse_line` — State Machine Explainer

---

## The Problem

CSV looks simple until it isn't. Consider this line:

```
1,"New York, ""the Big Apple""",active
```

A naive split on `,` gives:

```python
["1", '"New York', ' ""the Big Apple"""', "active"]  # WRONG — 4 fields, garbled
```

The correct parse produces:

```python
["1", 'New York, "the Big Apple"', "active"]  # CORRECT — 3 fields
```

| Field # | Raw in CSV                    | Parsed value              |
|---------|-------------------------------|---------------------------|
| 1       | `1`                           | `"1"`                     |
| 2       | `"New York, ""the Big Apple"""` | `'New York, "the Big Apple"'` |
| 3       | `active`                      | `"active"`                |

The comma inside the quoted field is data, not a delimiter. The doubled `""` is
an escaped quote character, not two quote characters. A simple character-by-character
scan has no way to distinguish these cases without remembering *context* — i.e.,
whether we are currently inside a quoted field or not.

That context is exactly what a **finite state machine** tracks.

---

## Quote Escaping: Why `""` Means `"`

> Defined in **RFC 4180** — the closest thing CSV has to a formal standard:
> https://www.rfc-editor.org/rfc/rfc4180

CSV has no backslash escaping. You cannot write `\"` to represent a literal quote
character inside a field — there is no such rule in the format.

Instead, CSV uses **quote doubling**: if you are inside a quoted field and need to
include a literal `"` character in the value, you write it as two consecutive quote
characters `""`. The parser sees the first `"` and cannot immediately decide if it
is the closing quote of the field or the start of an escape sequence. It resolves
this by **looking one character ahead**:

```
Inside IN_QUOTED state, on seeing '"':

  next char is also '"'  →  escape sequence: emit one '"' into buf, skip both
  next char is delimiter/\0    →  closing quote:   field is done, emit buf
  next char is anything  →  malformed CSV:   raise ValueError
```

There is no third interpretation. The format is unambiguous because these three
cases cover every possibility exhaustively.

**Concrete mapping:**

| Bytes in the CSV file | What ends up in the parsed field |
|-----------------------|----------------------------------|
| `"hello"`             | `hello`        (quotes stripped)                 |
| `"say ""hi"""`        | `say "hi"`     (each `""` → one `"`)             |
| `"a ""b"" c"`         | `a "b" c`      (two escapes in one field)         |
| `""`                  | `` (empty string) (open + immediately close)      |

### How the lookahead resolves trailing `"""`

Consider the field `"ABC"""` — three quotes at the end looks ambiguous at first glance.

```
index:  0    1    2    3    4    5    6    7
char:   "    A    B    C    "    "    "   \0
        │                   └────┘    │
        opening quote       escaped   closing
                            quote     quote
                            (one ")
```

The parser never sees three quotes as a unit. It processes them two decisions:

1. At p=4: sees `"`, looks at p+1=5 which is also `"` → **escape rule** fires →
   emit one `"` into buf, skip p=5, advance to p=6
2. At p=6: sees `"`, looks at p+1=7 which is `\0` → **closing quote rule** fires →
   emit the field

The lookahead makes the format unambiguous: the first `"` seen in IN_QUOTED always
consumes exactly one or two characters depending solely on what follows it. There is
no backtracking, no ambiguity, no third interpretation.

**Why not backslash?**

The CSV format predates any standard and was designed to be editable in spreadsheets
and plain text editors. Doubling the delimiter character is a convention borrowed
from SQL string literals (`''` for a literal `'` inside a `'...'` string). It
requires no special knowledge of escape sequences and is visually unambiguous once
you know the rule.

---

## The State Machine

The parser defines three states:

```
FIELD_START   — at the beginning of a new field, no characters consumed yet
IN_FIELD      — inside an unquoted field, accumulating characters
IN_QUOTED     — inside a quoted field, accumulating characters
```

### Transition diagram

```
                         quote char
              ┌─────────────────────────────────┐
              │                                 ▼
         ┌────────────┐   non-quote,      ┌──────────┐
 start ──►FIELD_START │   non-delimiter   │ IN_FIELD │
         └────────────┘ ─────────────────►└──────────┘
              │  ▲                             │  ▲
delimiter/\0 │  │ delimiter/\0                │  │ regular char
  (emit field)│  │ (emit field,reset)          │  │
              ▼  │                             └──┘
           [emit ""]    delimiter or \0
                        (emit field, reset)
                              │
         ┌───────────┐        │
         │ IN_QUOTED │◄───────┘ ← quote char from FIELD_START
         └───────────┘
              │  ▲
              │  │ regular char (including comma, space, newline)
              └──┘
              │
              │ "" (doubled quote) → emit single ", stay IN_QUOTED
              │
              │ " followed by delimiter or \0 → emit field, → FIELD_START
              │
              │ " followed by anything else → ValueError (malformed)
              │
              │ \0 with no closing quote → ValueError (unclosed quote)
```

### State transition table

| T#  | PREV_STATE                         | State        | Character seen          | Character category | Action                                      | Next state        |
|-----|------------------------------------|--------------|-------------------------|--------------------|---------------------------------------------|-------------------|
| T1  | FIELD_START / IN_FIELD / IN_QUOTED | FIELD_START  | `"`                     | quote char         | nothing (start of quoted field)             | IN_QUOTED         |
| T2  | FIELD_START / IN_FIELD / IN_QUOTED | FIELD_START  | `,` (configured delim)  | delimiter          | emit empty field `""`                       | FIELD_START       |
| T3  | FIELD_START / IN_FIELD / IN_QUOTED | FIELD_START  | `\0`                    | end of string      | emit empty field `""`, done                 | PROCESS_COMPLETED |
| T4  | FIELD_START / IN_FIELD / IN_QUOTED | FIELD_START  | `a`, `1`, `x`, …        | any other char     | BUF_APPEND(c)                               | IN_FIELD          |
| T5  | FIELD_START / IN_FIELD             | IN_FIELD     | `,` (configured delim)  | delimiter          | emit buf as field, reset buf                | FIELD_START       |
| T6  | FIELD_START / IN_FIELD             | IN_FIELD     | `\0`                    | end of string      | emit buf as field, done                     | PROCESS_COMPLETED |
| T7  | FIELD_START / IN_FIELD             | IN_FIELD     | `a`, `1`, `x`, …        | any other char     | BUF_APPEND(c)                               | IN_FIELD          |
| T8  | FIELD_START / IN_QUOTED            | IN_QUOTED    | `\0`                    | end of string      | raise `ValueError("unclosed quote")`        | PROCESS_COMPLETED |
| T9  | FIELD_START / IN_QUOTED            | IN_QUOTED    | `"` (next char: `"`)    | escaped quote      | BUF_APPEND(`"`), skip extra `"`             | IN_QUOTED         |
| T10 | FIELD_START / IN_QUOTED            | IN_QUOTED    | `"` (next char: `,`)    | closing quote      | emit buf as field, reset buf, advance p     | FIELD_START       |
| T11 | FIELD_START / IN_QUOTED            | IN_QUOTED    | `"` (next char: `\0`)   | closing quote      | emit buf as field, done                     | PROCESS_COMPLETED |
| T12 | FIELD_START / IN_QUOTED            | IN_QUOTED    | `"` (next char: other)  | malformed quote    | raise `ValueError("unexpected char")`       | PROCESS_COMPLETED |
| T13 | FIELD_START / IN_QUOTED            | IN_QUOTED    | `a`, `1`, `,`, ` `, …   | any other char     | BUF_APPEND(c)                               | IN_QUOTED         |

---

## Worked Example

**Input line:**

```
1,"New York, ""the Big Apple""",active
```

**Expected output:**

```python
["1", 'New York, "the Big Apple"', "active"]
```

**Character-by-character trace:**

```
p   char   state         buf              action
─── ────── ─────────     ──────────────── ──────────────────────────────────────
0   '1'    FIELD_START   []               not quote/delimiter → BUF_APPEND('1'), → IN_FIELD
1   ','    IN_FIELD      ['1']            delimiter → emit "1", reset buf, → FIELD_START
2   '"'    FIELD_START   []               quote → → IN_QUOTED
3   'N'    IN_QUOTED     []               BUF_APPEND('N')
4   'e'    IN_QUOTED     ['N']            BUF_APPEND('e')
5   'w'    IN_QUOTED     ['Ne']           BUF_APPEND('w')
6   ' '    IN_QUOTED     ['New']          BUF_APPEND(' ')
7   'Y'    IN_QUOTED     ['New ']         BUF_APPEND('Y')
8   'o'    IN_QUOTED     ['New Y']        BUF_APPEND('o')
9   'r'    IN_QUOTED     ['New Yo']       BUF_APPEND('r')
10  'k'    IN_QUOTED     ['New Yor']      BUF_APPEND('k')
11  ','    IN_QUOTED     ['New York']     comma is data inside quotes → BUF_APPEND(',')
12  ' '    IN_QUOTED     ['New York,']    BUF_APPEND(' ')
13  '"'    IN_QUOTED     ['New York, ']   next char (p=14) is also '"' → doubled quote
                                          → BUF_APPEND('"'), skip p=14
15  't'    IN_QUOTED     ['New York, "']  BUF_APPEND('t')
16  'h'    IN_QUOTED     [...]            BUF_APPEND('h')
17  'e'    IN_QUOTED     [...]            BUF_APPEND('e')
18  ' '    IN_QUOTED     [...]            BUF_APPEND(' ')
19  'B'    IN_QUOTED     [...]            BUF_APPEND('B')
20  'i'    IN_QUOTED     [...]            BUF_APPEND('i')
21  'g'    IN_QUOTED     [...]            BUF_APPEND('g')
22  ' '    IN_QUOTED     [...]            BUF_APPEND(' ')
23  'A'    IN_QUOTED     [...]            BUF_APPEND('A')
24  'p'    IN_QUOTED     [...]            BUF_APPEND('p')
25  'p'    IN_QUOTED     [...]            BUF_APPEND('p')
26  'l'    IN_QUOTED     [...]            BUF_APPEND('l')
27  'e'    IN_QUOTED     ['New York, "the Big Apple']  BUF_APPEND('e')
28  '"'    IN_QUOTED     [...]            next char (p=29) is also '"' → doubled quote
                                          → BUF_APPEND('"'), skip p=29
30  '"'    IN_QUOTED     ['New York, "the Big Apple"']
                                          next char (p=31) is ',' (delimiter)
                                          → emit field, reset buf, advance p past ','
                                          → FIELD_START
32  'a'    FIELD_START   []               not quote/delimiter → BUF_APPEND('a'), → IN_FIELD
33  'c'    IN_FIELD      ['a']            BUF_APPEND('c')
34  't'    IN_FIELD      ['ac']           BUF_APPEND('t')
35  'i'    IN_FIELD      ['act']          BUF_APPEND('i')
36  'v'    IN_FIELD      ['acti']         BUF_APPEND('v')
37  'e'    IN_FIELD      ['activ']        BUF_APPEND('e')
38  '\0'   IN_FIELD      ['active']       end of string → emit "active" → done
```

**Result:** `["1", 'New York, "the Big Apple"', "active"]` ✓

---

## Transition Coverage Examples

Five short inputs that together hit every row (T1–T13) in the table.

---

### Example A — `a,"say ""hi""",`

| T#  | PREV_STATE   | State        | Character seen              | Character category | Action                              | Next state        |
|-----|--------------|--------------|-----------------------------|--------------------|-------------------------------------|-------------------|
| T4  | —            | FIELD_START  | `a`                         | any other char     | BUF_APPEND(c)                       | IN_FIELD          |
| T5  | IN_FIELD     | IN_FIELD     | `,`                         | delimiter          | emit buf as field, reset buf        | FIELD_START       |
| T1  | FIELD_START  | FIELD_START  | `"`                         | quote char         | nothing (start of quoted field)     | IN_QUOTED         |
| T13 | IN_QUOTED    | IN_QUOTED    | `s`, `a`, `y`, ` `, `h`, `i`| any other char     | BUF_APPEND(c)                       | IN_QUOTED         |
| T9  | IN_QUOTED    | IN_QUOTED    | `"` (next: `"`)             | escaped quote      | BUF_APPEND(`"`), skip extra `"`     | IN_QUOTED         |
| T10 | IN_QUOTED    | IN_QUOTED    | `"` (next: `,`)             | closing quote      | emit buf as field, advance p        | FIELD_START       |
| T3  | FIELD_START  | FIELD_START  | `\0`                        | end of string      | emit empty field `""`, done         | PROCESS_COMPLETED |

```
p   char  state        action
0   a     FIELD_START  other char → BUF_APPEND('a')            [T4]
1   ,     IN_FIELD     delimiter → emit "a", reset buf         [T5]
2   "     FIELD_START  quote → start quoted field              [T1]
3   s     IN_QUOTED    BUF_APPEND('s')                         [T13]
4   a     IN_QUOTED    BUF_APPEND('a')                         [T13]
5   y     IN_QUOTED    BUF_APPEND('y')                         [T13]
6   ' '   IN_QUOTED    BUF_APPEND(' ')                         [T13]
7   "     IN_QUOTED    next=='"' → doubled quote, emit '"'     [T9]
8   "     (consumed by T9 lookahead — skipped)
9   h     IN_QUOTED    BUF_APPEND('h')                         [T13]
10  i     IN_QUOTED    BUF_APPEND('i')                         [T13]
11  "     IN_QUOTED    next=='"' → doubled quote, emit '"'     [T9]
12  "     (consumed by T9 lookahead — skipped)
13  "     IN_QUOTED    next==',' → emit 'say "hi"', advance p  [T10]
14  ,     (consumed by T10 lookahead — p advanced past it)
15  \0    FIELD_START  \0 → emit "", done                      [T3]
```

Result: `["a", 'say "hi"', ""]` ✓

---

### Example B — `abc`

| T#  | PREV_STATE   | State        | Character seen  | Character category | Action                              | Next state        |
|-----|--------------|--------------|-----------------|---------------------|-------------------------------------|-------------------|
| T4  | —            | FIELD_START  | `a`             | any other char      | BUF_APPEND(c)                       | IN_FIELD          |
| T7  | IN_FIELD     | IN_FIELD     | `b`, `c`        | any other char      | BUF_APPEND(c)                       | IN_FIELD          |
| T6  | IN_FIELD     | IN_FIELD     | `\0`            | end of string       | emit buf as field, done             | PROCESS_COMPLETED |

```
p   char  state        action
0   a     FIELD_START  other char → BUF_APPEND('a')            [T4]
1   b     IN_FIELD     other char → BUF_APPEND('b')            [T7]
2   c     IN_FIELD     other char → BUF_APPEND('c')            [T7]
3   \0    IN_FIELD     \0 → emit "abc", done                   [T6]
```

Result: `["abc"]` ✓

---

### Example C — `,"hello"`

| T#  | PREV_STATE   | State        | Character seen              | Character category | Action                              | Next state        |
|-----|--------------|--------------|-----------------------------|--------------------|-------------------------------------|-------------------|
| T2  | —            | FIELD_START  | `,`                         | delimiter          | emit empty field `""`               | FIELD_START       |
| T1  | FIELD_START  | FIELD_START  | `"`                         | quote char         | nothing (start of quoted field)     | IN_QUOTED         |
| T13 | IN_QUOTED    | IN_QUOTED    | `h`, `e`, `l`, `l`, `o`     | any other char     | BUF_APPEND(c)                       | IN_QUOTED         |
| T11 | IN_QUOTED    | IN_QUOTED    | `"` (next: `\0`)            | closing quote      | emit buf as field, done             | PROCESS_COMPLETED |

```
p   char  state        action
0   ,     FIELD_START  delimiter → emit ""                     [T2]
1   "     FIELD_START  quote → start quoted field              [T1]
2   h     IN_QUOTED    BUF_APPEND('h')                         [T13]
3   e     IN_QUOTED    BUF_APPEND('e')                         [T13]
4   l     IN_QUOTED    BUF_APPEND('l')                         [T13]
5   l     IN_QUOTED    BUF_APPEND('l')                         [T13]
6   o     IN_QUOTED    BUF_APPEND('o')                         [T13]
7   "     IN_QUOTED    next=='\0' → emit "hello", done         [T11]
```

Result: `["", "hello"]` ✓

---

### Example D — `"unclosed` *(error)*

| T#  | PREV_STATE   | State        | Character seen                      | Character category | Action                              | Next state        |
|-----|--------------|--------------|-------------------------------------|--------------------|-------------------------------------|-------------------|
| T1  | —            | FIELD_START  | `"`                                 | quote char         | nothing (start of quoted field)     | IN_QUOTED         |
| T13 | IN_QUOTED    | IN_QUOTED    | `u`, `n`, `c`, `l`, `o`, `s`, `e`, `d` | any other char | BUF_APPEND(c)                       | IN_QUOTED         |
| T8  | IN_QUOTED    | IN_QUOTED    | `\0`                                | end of string      | raise `ValueError("unclosed quote")`| PROCESS_COMPLETED |

```
p   char  state        action
0   "     FIELD_START  quote → start quoted field              [T1]
1   u     IN_QUOTED    BUF_APPEND('u')                         [T13]
2   n     IN_QUOTED    BUF_APPEND('n')                         [T13]
3   c     IN_QUOTED    BUF_APPEND('c')                         [T13]
4   l     IN_QUOTED    BUF_APPEND('l')                         [T13]
5   o     IN_QUOTED    BUF_APPEND('o')                         [T13]
6   s     IN_QUOTED    BUF_APPEND('s')                         [T13]
7   e     IN_QUOTED    BUF_APPEND('e')                         [T13]
8   d     IN_QUOTED    BUF_APPEND('d')                         [T13]
9   \0    IN_QUOTED    \0 inside quoted field → ValueError     [T8]
```

Raises: `ValueError: mycsv: unclosed quote` ✓

---

### Example E — `"bad"x` *(error)*

| T#  | PREV_STATE   | State        | Character seen          | Character category | Action                               | Next state        |
|-----|--------------|--------------|-------------------------|--------------------|--------------------------------------|-------------------|
| T1  | —            | FIELD_START  | `"`                     | quote char         | nothing (start of quoted field)      | IN_QUOTED         |
| T13 | IN_QUOTED    | IN_QUOTED    | `b`, `a`, `d`           | any other char     | BUF_APPEND(c)                        | IN_QUOTED         |
| T12 | IN_QUOTED    | IN_QUOTED    | `"` (next: `x`)         | malformed quote    | raise `ValueError("unexpected char")`| PROCESS_COMPLETED |

```
p   char  state        action
0   "     FIELD_START  quote → start quoted field              [T1]
1   b     IN_QUOTED    BUF_APPEND('b')                         [T13]
2   a     IN_QUOTED    BUF_APPEND('a')                         [T13]
3   d     IN_QUOTED    BUF_APPEND('d')                         [T13]
4   "     IN_QUOTED    next=='x' → ValueError                  [T12]
```

Raises: `ValueError: mycsv: unexpected char after closing quote` ✓

---

### Coverage matrix

| Transition | Description                        | Covered by  |
|------------|------------------------------------|-------------|
| T1         | FIELD_START + `"` → IN_QUOTED      | A, C, D, E  |
| T2         | FIELD_START + delimiter → FIELD_START | C        |
| T3         | FIELD_START + `\0` → done          | A           |
| T4         | FIELD_START + other → IN_FIELD     | A, B        |
| T5         | IN_FIELD + delimiter → FIELD_START | A           |
| T6         | IN_FIELD + `\0` → done             | B           |
| T7         | IN_FIELD + other → IN_FIELD        | B           |
| T8         | IN_QUOTED + `\0` → ValueError      | D           |
| T9         | IN_QUOTED + `""` → IN_QUOTED       | A           |
| T10        | IN_QUOTED + `"` + delim → FIELD_START | A        |
| T11        | IN_QUOTED + `"` + `\0` → done      | C           |
| T12        | IN_QUOTED + `"` + other → ValueError | E         |
| T13        | IN_QUOTED + other → IN_QUOTED      | A, C, D, E  |

---

## Why a buffer is necessary

`parse_line_no_quote` can use zero-copy pointer arithmetic — it slices directly into
the original string because no character transformation occurs.

`parse_line` **cannot** do this. When it encounters `""` it must emit a single `"`
into the output. The output field is a *transformed* version of the input bytes, so
it must be assembled character-by-character into a separate buffer. That is the
entire reason `PyMem_Malloc` / `BUF_APPEND` exists in `parse_line` but not in
`parse_line_no_quote`.
