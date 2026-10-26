# compare_strlen.py の説明

「word 単位で読む strlen は、インライン化され、書き込み型が非互換のとき strict aliasing で壊れうる。`__may_alias__` はそれを防ぐ」を、実際にコンパイルして実行して示すスクリプトです。`.c` ファイルには依存せず、`.py` 1つで完結します(C のソースは `.py` に埋め込み済み)。

```
python3 compare_strlen.py          # コンパイラを変えるときは CC=clang など
```

前提: Linux x86_64(リトルエンディアン)+ GCC。`uint64_t` が `unsigned long`(= `size_t`)であることも前提です。x86_64 以外では警告を出します。

各節の末尾に出典を付けました。出典の略記は末尾の「出典一覧」にまとめてあります。「手元の実行結果」(5 章)は、このスクリプトを GCC 10.5.0 / x86_64 Linux で実際に動かした観測であり、出典ではありません。

出典: [x86-64 psABI]（Scalar Types の表。LP64 では `unsigned long` と `size_t` はどちらも 8 バイト）

## 0. 背景(このスクリプトが確かめたいこと)

glibc や musl の C 版 strlen は、1 バイトずつではなく 1 word(64bit 環境では 8 バイト)ずつ読んでゼロバイトを探します。このとき `char` の配列を `size_t` などの型で読むため、素直に書くと C の規則(strict aliasing: 互換性のない型のポインタ経由では同じメモリにアクセスしないとみなしてよい)に反します。実際の glibc(`op_t`)と musl(`word`)は、読む型に `__may_alias__` を付けてこの問題を避けています。このスクリプトは、その `__may_alias__` を外すと何が起きるかを確かめるものです。

規則に反しても普通は動きますが、コンパイラは「`uint32_t` で書いたメモリを `size_t` で読むことはない」と仮定して最適化できます。たとえば、`uint32_t` のストアしかないループの外へ `size_t` のロード(メモリ読み出し)を移動すると、ループ内で書き換えた内容を読まず、古い内容を使ってしまいます。壊れうるのは、次の2つが揃ったときです。

1. strlen が呼び出し側にインライン展開される(同一ファイル内、または LTO(リンク時最適化))。展開されないと、strlen は中身の分からない関数で、コンパイラは並べ替えの推論ができません。
2. 対象メモリが `char` 以外で、`size_t` と互換性のない型(例: `uint32_t`)で書かれている。

`__attribute__((__may_alias__))` を付けた型で読むと、コンパイラはその型を「何とでも alias しうる」(`char` と同じ扱い)とみなし、上の最適化をしなくなります。このスクリプトは、実際にコンパイルして実行し、その違いを示します。ただし C の規則上は未定義動作(UB)なので、「壊れうる」であって「必ず壊れる」ではありません。

出典: [C23] 6.5.1p6・p7（オブジェクトの実効型と、格納された値にアクセスしてよい lvalue の型の一覧。`char` 系の型は例外）、脚注 85（この一覧の意図は「エイリアスしうる場合としえない場合を定めること」）、附属書 J.2 の (36)（一覧に反するアクセスは未定義動作）、[GCC] `-fstrict-aliasing`（"accessing an object of one type via an expression of a different type is not allowed, unless the types are compatible types, differ only in signedness or qualifiers, or the expression has a character type"。`-O2`・`-O3`・`-Os` で有効）、[GCC] `may_alias`（"Accesses through pointers to types with this attribute are not subject to type-based alias analysis"。"an lvalue expression dereferencing such a pointer is treated like having a character type"）、[CERT EXP39-C]、[glibc strlen.c]・[glibc string-optype.h]（`typedef unsigned long int __attribute__ ((__may_alias__)) op_t;`）、[musl strlen.c]（`typedef size_t __attribute__((__may_alias__)) word;`）、[Bit Twiddling Hacks]（"Determine if a word has a zero byte"。32 ビット版の `haszero(v)` は `(((v) - 0x01010101UL) & ~(v) & 0x80808080UL)`）

## 1. 比較している3つの実装

| 名前 | 中身 |
|---|---|
| `bytewise` | `ft_strlen.c` そのまま。1バイトずつ数える(基準として1回だけ実行) |
| `sizet` | `ft_strlen_sizet.c` そのまま。先頭をアラインしたあと `const size_t *` で 8 バイトずつ読みゼロバイトを探す。**may_alias なし** |
| `mayalias` | `sizet` の `const size_t *wide_s` だけを `const alias_size_t *wide_s` に置き換えたもの。`alias_size_t` は `typedef size_t __attribute__((__may_alias__)) alias_size_t;` |

出典: [musl strlen.c]（同じ形の `typedef size_t __attribute__((__may_alias__)) word;`）、[GCC] `may_alias`、[C23] 7.26.6.4（`strlen` は終端のヌル文字の手前の文字数を返す）

## 2. 1つの条件で何をしているか

準備として、64 バイトのバッファ `pool` 全体を `'A'` で埋め、末尾の 1 バイトだけ 0 にします。その上で、長さ N = 7, 8, 9 の順に、次の 1〜3 を繰り返します(`lens[k]` が N)。

バッファを `'A'` で埋めておくのは、コンパイラが `size_t` ロードをループの外へ巻き上げた場合に、古い内容(先頭 word にゼロ無し)で判定されて結果に現れるようにするためです。バッファは `__builtin_assume_aligned(pool, 8)` で 8 バイト境界と示してあります(境界が不明だと strlen 先頭の 1 バイト読みループが残り、バグが隠れるため)。

出典: [GCC] `__builtin_assume_aligned`（"allows the compiler to assume that the returned pointer is at least align bytes aligned"）、[C23] 6.5.1p6（`uint32_t` などの型で書いたとき、その型がオブジェクトの実効型になる）

1. **`char` の配列 `b[16]` に、N 文字の文字列 + ゼロ終端を作る。** `b` は `unsigned char` なので、型の問題は起きません。
```c
unsigned char b[16];
for (i = 0; i < 16; i++)
    b[i] = i < lens[k] ? (unsigned char)('A' + i % 26) : 0;
```
2. **`b[]` を、書き込み型 T の単位に詰めて、`pool` の先頭 16 バイトにストアする**(T は `char` / `uint32_t` / `uint64_t`)。ここが「T で書く」にあたる処理です。以下は `uint32_t` の例です。
```c
uint32_t *buf = __builtin_assume_aligned(pool, 8);   // pool は 64 バイトのバッファ
for (i = 0; i < 16 / 4; i++)                          // 4 回
{
    uint32_t v = 0;
    for (j = 0; j < 4; j++)
        v |= (uint32_t)b[4 * i + j] << (8 * j);       // リトルエンディアンで 4 バイトを 1 個にまとめる
    buf[i] = v;                                       // uint32_t のストア
}
```
3. **そのバッファを strlen に渡し、返り値を記録する。** 正しければ、返り値は N になる。

`char` なら 1 バイトずつ、`uint64_t` なら 8 バイトずつ、同じ形のコードで詰めます(型とバイト数だけが変わります)。書き込む範囲は常に先頭 16 バイトで、`pool` の残り 48 バイトは `'A'` のままです。

### 例: N = 7、T = `uint32_t`

手順 1 で作る `b[]`:

```
index: 0 1 2 3 4 5 6 7  8 9 ... 15
b[]  : A B C D E F G \0 \0 \0 ... \0
```

手順 2 の 4 回のストア(各回の `v` はリトルエンディアンで詰めた値):

| i | 詰めるバイト | `buf[i]` に書く値 |
|---|---|---|
| 0 | `A B C D` | `0x44434241` |
| 1 | `E F G \0` | `0x00474645` |
| 2 | `\0 \0 \0 \0` | `0x00000000` |
| 3 | `\0 \0 \0 \0` | `0x00000000` |

メモリ上のバイト列は `"ABCDEFG\0\0..."` で、`char` で書いた場合と同じ内容です。違うのは、書き込みが `uint32_t` 型のストアだという点だけです。

N = 7 では先頭の 8 バイト(strlen が最初に読む word)の中にゼロが含まれます。N = 8, 9 では先頭 word は全部文字で、ゼロは 2 語目に現れます。この違いが、5 章の `8*, 8, 9`(7 だけ誤る)の理由に関わります。

書き込み型の意味:

- `char`: 何とでも alias できる型。壊れないはず。
- `uint64_t`: x86_64 Linux では `size_t` と同じ型。壊れないはず。
- `uint32_t`: `size_t` と非互換。壊れうる側。

出典: [C23] 6.5.1p7（互換な型、または文字型でのアクセスだけが許される）、[GCC] `-fstrict-aliasing`（"A character type may alias any other type."、"an unsigned int can alias an int, but not a void* or a double"）、[x86-64 psABI]（`unsigned long` と `size_t` が同じ幅）

## 3. 条件の行列(3 × 4 × 2 × 3 = 72 条件 + 基準1回)

| 軸 | 値 |
|---|---|
| strlen の置き方(表) | `inline`(同じ翻訳単位、`always_inline`)/ `noinline`(同じ翻訳単位、`noinline`)/ `sepTU+LTO`(別ファイル + `-flto`) |
| ビルドオプション(列) | `(none)`(指定なし = gcc では `-O0` 相当)/ `-O2` / `-O3` / `-O2 -fno-strict-aliasing` |
| 実装(行) | `sizet` / `mayalias` |
| 書き込み型(行) | `char` / `uint32_t` / `uint64_t` |

出典: [GCC] `-O0`（"This is the default."）、[GCC] `-fstrict-aliasing`（`-O2`・`-O3` で有効。`-fno-strict-aliasing` で無効にできる）、[GCC] `-fipa-strict-aliasing`（関数境界をまたいでも strict aliasing の規則を適用する。既定で有効）、[GCC] `always_inline`・`noinline`（関数属性）、[GCC] `-flto`（"all the function bodies are read from these ELF sections and instantiated as if they had been part of the same translation unit"。別ファイルの関数も同じ翻訳単位のようにインライン展開できる）

## 4. 出力の読み方

### 4.1 冒頭

コンパイラのバージョンと CPU、検証内容の短い説明。

### 4.2 表(strlen の置き方ごとに1つ、計3つ)

```
=== strlen: inline ===
impl / writer       (none)        -O2           -O3           -O2 -fno-strict-aliasing
sizet / uint32_t    7, 8, 9       8*, 8, 9      8*, 8, 9      7, 8, 9
...
```

- 行は「実装 / 書き込み型」の 6 行、列はビルドオプション。**全セルを省略せず表示します**(OK の行が、壊れた行の対照になります)。
- `bytewise` は `char` でしか読まず壊れようがないため、表には含めません。
- セルには、N = 7, 8, 9 に対する strlen の返り値を順に並べる(正しければ `7, 8, 9`)。期待と違う値には `*` が付く(例: `8*, 8, 9` は、長さ 7 の文字列に 8 を返した)。
- 異常終了したときは、セルが `CRASH`(シグナル)または `HANG`(5 秒以内に終わらない)になる。

### 4.3 末尾

```
基準: 1バイトずつ読む bytewise ... を inline, -O3, uint32_t 書き込みで1回だけ実行 → 7, 8, 9
誤った長さを返した条件: 3 / 72   (* が付いたセルが誤り)
```

- 「基準」の行は、`bytewise` をスクリプトの動作確認として1条件だけ実行した結果です。ここが `7, 8, 9` なら、バッファの詰め方やテストの書き方は正しいと言えます。
- 「誤った長さを返した条件」は、`*` が1つでも付いたセル(1セル = 1条件)の数です。

### 4.4 証拠(壊れた条件があるときだけ自動で出る)

最初に壊れた `inline` の条件のビルドオプションで、コンパイラが最適化したあとのコードを `sizet` と `mayalias` で並べて表示します。「なぜ壊れたか」を表の数字だけでなく、コンパイラの中身で確かめるためのものです。内容の読み方は 5 章で説明します。

出典: [GCC] `-fdump-tree-switch`（GCC 開発者オプション。"By default, trees are pretty-printed into a C-like representation."。`-fdump-tree-optimized` は最適化を終えた段階のダンプ）

## 5. 現在の結果(GCC 10.5.0 / x86_64)

誤りは 72 条件中 3 条件で、すべて `sizet`(may_alias なし)× `uint32_t` 書き込みです。誤りの中身は、いずれも `8*, 8, 9`(長さ 7 の文字列に 8 を返す)で、長さ 8 と 9 は正しく返りました。

| 置き方 | ビルドオプション | 結果 |
|---|---|---|
| `inline` | `-O2` / `-O3` | `8*, 8, 9` |
| `sepTU+LTO` | `-O3` | `8*, 8, 9` |
| 上のどれも | `-O2 -fno-strict-aliasing` | 正しい |
| `noinline` | すべて | 正しい(関数境界が壁になる) |
| 任意 | `(none)` | 正しい |
| 任意 | `char` / `uint64_t` 書き込み | 正しい |
| 任意 | `mayalias` | 正しい |

### 5.1 `8*, 8, 9` になる理由

- 先頭 word の `size_t` ロードがループの外へ移り、`'A'` だけの古い内容で「ゼロ無し」と判定される。
- そのため word ループが先頭 word を飛ばして 2 語目へ進み、長さ 8 を返す。
- 長さ 8 と 9 は、ゼロが 2 語目にあるため、先頭 word を飛ばしても正しい結果になる。

出典: 「ロードがループの外へ移った」ことは、5.2 のコンパイラ出力(手元の観測)から読み取った事実です。コンパイラがそうしてよい根拠は、[C23] 6.5.1p7 と [GCC] `-fstrict-aliasing`(型の違うストアはロードに影響しないとみなしてよい)です。

### 5.2 出力されるコンパイラ内部の表現の読み方

証拠として出力されるのは、GCC が最適化を終えた段階の中間表現(GIMPLE。`-fdump-tree-optimized` で出力)です。C に近い形の疑似コードで、次のように読めば足ります。

| 表記 | 意味 |
|---|---|
| `<bb 4>` | 基本ブロック 4。途中に分岐を含まない一続きの処理のかたまり。`goto <bb N>` でほかのブロックへ移る |
| `_107`, `pretmp_96` など | コンパイラが作った一時変数(`_数字` は番号だけの名前) |
| `MEM[(const size_t *)buf_30]` | アドレス `buf_30` から 8 バイトを `size_t` として**読み出す**(メモリロード) |
| `# ivtmp.279_28 = PHI <0(4), ...>` | ループの先頭で値を合流させる印。この行が出たら、ここからループが始まる |
| `18374403900871474943` | 16進で `0xFEFEFEFEFEFEFEFF`。8 バイトの各バイトから 1 を引く(`x - 0x0101...01`)ための値 |
| `9259542123273814144` | 16進で `0x8080808080808080`。各バイトの最上位ビットだけを取り出すマスク |
| `<-- size_t 単位のロード` | 読みやすさのためにスクリプトが付け足した印(GCC の出力ではない) |

strlen の word 単位の判定は `(x - 0x0101...01) & ~x & 0x8080...80` で、`x` の 8 バイトのどこかにゼロがあるときだけ 0 以外になります。出力中の `_107`〜`_110` の4行が、この計算そのものです。

出典: [GCC Internals GIMPLE]（GIMPLE は GENERIC から作る三アドレスコード形式の中間表現で、制御構造は条件ジャンプに落とされ、複雑な式は一時変数に分けられる）、[GCC] `-fdump-tree-switch`、[Bit Twiddling Hacks]（"Determine if a word has a zero byte" の式。32 ビット幅の定数 `0x01010101UL`・`0x80808080UL` を 64 ビット幅にしたものが上の 2 つの定数）、[musl strlen.c]（`#define HASZERO(x) ((x)-ONES & ~(x) & HIGHS)`）。`<bb N>`・`PHI` の読み方は上の表の説明であり、GCC の dump に付く表記の一般的な意味に基づく

以下のコードブロックは、実際の出力から、実行頻度の数値(`[local count: ...]`)や一部の補助的な行を省き、右側に説明(`<-`)を加えたものです。

#### `sizet`(may_alias なし)

```
<bb 2>:                                   <- 関数の入口
  buf_30 = pool_29(D);                    <- 書き込み先バッファのアドレス
  if (cnt_32(D) > 0)                      <- 試す長さが1つ以上あるなら
    goto <bb 4>;                             ループの準備(bb 4)へ
  else
    goto <bb 3>;                             なければ終了(bb 3)へ
<bb 3>:
  return;
<bb 4>:                                   <- ループに入る前
  pretmp_96 = MEM[(const size_t *)buf_30];   <-- size_t 単位のロード ★
  _107 = ~pretmp_96;                      <- ここから4行は、読んだ word に
  _108 = pretmp_96 + 18374403900871474943;   ゼロバイトがあるかの判定
  _109 = _107 & _108;
  _110 = _109 & 9259542123273814144;
<bb 5>:
  # ivtmp.279_28 = PHI <0(4), ...>        <- ここからループ(長さ7,8,9を順に試す)
```

★ の行が問題です。バッファの先頭 word を読む処理が、**ループより前**の bb 4 に出ています。ループの中で `uint32_t` で何度も書き換えているのに、読み出しは最初の1回(`'A'` だけの古い内容)で済ませています。コンパイラは「`uint32_t` のストアは `size_t` のロードに影響しない」と仮定して、このロードを外に出してよいと判断した(ループ不変式の巻き上げ)からです。この古い word には 0 バイトが無いので「ゼロ無し」と判定され、`8*, 8, 9` の原因になります。

#### `mayalias`(`__may_alias__` 付き)

```
<bb 2>:                                   <- 関数の入口
  buf_30 = pool_29(D);
  if (cnt_32(D) > 0)
    goto <bb 3>;
  else
    goto <bb 16>;
<bb 3>:
  _16 = (unsigned long) cnt_32(D);        <- 型変換だけ。メモリ読み出しは無い
<bb 4>:
  # ivtmp.129_46 = PHI <0(3), ...>        <- ここからループ
```

ループに入る前に `MEM[...]` の読み出しがありません。ロードはループの中に残っているので、毎回、書き換え後の内容を読みます。`__may_alias__` により、コンパイラが「`uint32_t` のストアが `size_t` のロードに影響するかもしれない」と判断するためです。

なお、出力はループの先頭(`PHI` の行)までで切ってあります。ループの中身は、この説明に必要ないため表示していません。

## 6. 限界・注意

- **GCC 10.5 での観測です。** 他のバージョンやコンパイラでは条件が変わりえます。
- UB なので「壊れうる」であって「必ず壊れる」ではありません。`sepTU+LTO` で `-O2` は壊れず `-O3` だけ壊れたように、最適化の判断次第です。
- 壊れる条件(`__builtin_assume_aligned`、事前に `'A'` で埋める、書き込みがループ内、ポインタ経由のバッファ)は、バグを顕在化させるために選んだものです。実際の libft の使い方がこの形になるとは限りません。ローカル配列(宣言)を直接書く形は、GCC 10.5 では TBAA が使われず壊れませんでした。
- 検証していないこと: `-Wstrict-aliasing` の警告の有無、AddressSanitizer による『文字列の終端より後ろを読んでしまう』問題の検出。
- 一時ファイルは `/tmp` 配下の `strlen_cmp_*` に作られ、終了時に削除されます。

出典: [C23] 附属書 J.2 の (36)（未定義動作なので、結果は規格では定まらず、コンパイラの最適化判断しだい）、[GCC] `-Wstrict-aliasing`（警告は strict aliasing の違反をすべて検出するわけではない旨の記述がある）

## 出典一覧

- `[C23]` ISO/IEC 9899:2024 作業草案 N3220（C23 とほぼ同じ本文）：[open-std.org の N3220](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3220.pdf)。条項番号はこの文書のもの
- `[CERT EXP39-C]` SEI CERT C Coding Standard「EXP39-C. Do not access a variable through a pointer of an incompatible type」：[SEI CERT Wiki](https://wiki.sei.cmu.edu/confluence/display/c/EXP39-C.+Do+not+access+a+variable+through+a+pointer+of+an+incompatible+type)
- `[GCC]` Using the GNU Compiler Collection（GCC 17 開発版のマニュアル。節番号はこの版のもの。オンライン版は <https://gcc.gnu.org/onlinedocs/gcc/>）
  - `may_alias`・`always_inline`・`noinline`：6.4.1 Common Attributes（[オンライン版](https://gcc.gnu.org/onlinedocs/gcc/Common-Attributes.html)）
  - `-O0`・`-fstrict-aliasing`・`-fipa-strict-aliasing`・`-flto`：3.12 Options That Control Optimization（[オンライン版](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)）
  - `-Wstrict-aliasing`：3.9 Options to Request or Suppress Warnings（[オンライン版](https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html)）
  - `-fdump-tree-switch`：3.19 GCC Developer Options（[オンライン版](https://gcc.gnu.org/onlinedocs/gcc/Developer-Options.html)）
  - `__builtin_assume_aligned`：7.12 Other Built-in Functions Provided by GCC（[オンライン版](https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html)）
- `[GCC Internals GIMPLE]` GNU Compiler Collection Internals「GIMPLE」：[gcc.gnu.org](https://gcc.gnu.org/onlinedocs/gccint/GIMPLE.html)
- `[glibc strlen.c]` glibc の汎用 C 版 strlen（GitHub のミラー）：[string/strlen.c](https://github.com/bminor/glibc/blob/master/string/strlen.c)
- `[glibc string-optype.h]` glibc の word 型 `op_t` の定義：[sysdeps/generic/string-optype.h](https://github.com/bminor/glibc/blob/master/sysdeps/generic/string-optype.h)
- `[musl strlen.c]` musl libc の strlen：[src/string/strlen.c](https://git.musl-libc.org/cgit/musl/tree/src/string/strlen.c)
- `[Bit Twiddling Hacks]` Sean Eron Anderson, "Bit Twiddling Hacks"：[graphics.stanford.edu/~seander/bithacks.html](https://graphics.stanford.edu/~seander/bithacks.html)
- `[x86-64 psABI]` System V ABI の x86-64 の補足（Scalar Types の表は `x86-64-ABI/low-level-sys-info.tex`）：[GitLab のリポジトリ](https://gitlab.com/x86-psABIs/x86-64-ABI)
